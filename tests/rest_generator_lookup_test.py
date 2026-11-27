"""Protocol-level REST lookup regressions. Build the REST generator before running."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import unittest


def u32(value):
    return struct.pack('<I', value)


def text(value):
    encoded = value.encode('utf-8')
    return u32(len(encoded)) + encoded


def array(values):
    return u32(len(values)) + b''.join(values)


LOCATION = text('lookup.cx') + struct.pack('<4i', 1, 1, 1, 2)
BOX = 'LookupFixture.Box`1'
CONTRACT = 'LookupFixture.IApi'
METHOD = CONTRACT + '.Get()'


def annotation(name, target, names=(), values=()):
    return (text(name) + text(target) + array([text(n) for n in names])
            + array([text(v) for v in values]) + LOCATION)


def descriptor(identity, metadata, kind, members=(), generic=False):
    return (b''.join(text(s) for s in [identity, metadata, 'LookupFixture',
                                    'LookupFixture', 'Public', kind])
            + bytes([0, 0, 0, generic])
            + array([text('T')] if generic else []) + array([]) + array([])
            + array(members) + LOCATION)


def request(definition, closed, argument):
    return (text(definition) + text(closed) + text('LookupFixture')
            + array([text(argument)]) + LOCATION)


def snapshot(result, requests, extra_annotations=(), extra_types=()):
    member = (b''.join(text(s) for s in [METHOD, 'Get',
              'cxcore.System.Threading.Tasks.Task<' + result + '>', 'Public', 'Method'])
              + bytes(3) + array([]) + LOCATION + bytes(1))
    annotations = [annotation('System.Data.Serializable', BOX),
                   annotation('cxnet.System.Net.Http.Rest.Get', METHOD,
                              ['route'], ['"/value"'])]
    annotations += list(extra_annotations)
    types = list(extra_types) + [descriptor(BOX, 'Box`1', 'Class', generic=True),
                                descriptor(CONTRACT, 'IApi', 'Interface', [member])]
    factory = (text('cxnet.System.Net.Http.Rest.RestClient.For()')
               + text('For<IApi>') + text('cxnet.System.Net.Http.Rest.RestClient')
               + text('cxnet') + text('LookupFixture')
               + array([text(CONTRACT)]) + LOCATION)
    return (text('LookupFixture') + array([]) + array(annotations) + array(types)
            + array(requests) + array([factory]))


def frame(kind, payload=b''):
    # CXGN protocol v4: magic, version, kind, request ID, payload length.
    return struct.pack('<4sHHQI', b'CXGN', 4, kind, 1, len(payload)) + payload


def read_text(data, offset=0):
    size = struct.unpack_from('<I', data, offset)[0]
    end = offset + 4 + size
    return data[offset + 4:end].decode('utf-8'), end


class RestLookupTests(unittest.TestCase):
    def generate(self, payload):
        process = subprocess.run([str(GENERATOR)],
                                 input=frame(1) + frame(2, payload) + frame(4),
                                 capture_output=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr.decode(errors='replace'))
        sources, diagnostics, kinds = [], [], []
        offset = 0
        while offset < len(process.stdout):
            magic, version, kind, request_id, size = struct.unpack_from(
                '<4sHHQI', process.stdout, offset)
            self.assertEqual((magic, version, request_id), (b'CXGN', 4, 1))
            offset += 20
            payload = process.stdout[offset:offset + size]
            self.assertEqual(len(payload), size)
            offset += size
            kinds.append(kind)
            if kind == 5:
                _, start = read_text(payload)
                source, _ = read_text(payload, start)
                sources.append(source)
            elif kind == 6:
                diagnostic, _ = read_text(payload)
                diagnostics.append(diagnostic)
        self.assertEqual(kinds[0], 1)
        self.assertEqual(kinds[-1], 8)
        return '\n'.join(sources), diagnostics

    def test_closed_alias_and_definition_isolation(self):
        source, diagnostics = self.generate(snapshot('Alias', [
            request('Other.Box`1', 'Alias', 'System.Int32'),
            request(BOX, 'Alias', 'System.String')]))
        self.assertEqual(diagnostics, [])
        self.assertIn('Box_System_StringJsonCodec', source)
        self.assertNotIn('Box_System_Int32JsonCodec', source)

    def test_canonical_identity_and_whitespace(self):
        source, diagnostics = self.generate(snapshot('LookupFixture.Box<\tSystem.String >',
            [request(BOX, 'DifferentAlias', 'System.String')]))
        self.assertEqual(diagnostics, [])
        self.assertIn('Box_System_StringJsonCodec', source)

    def test_first_request_wins_across_aliases(self):
        source, diagnostics = self.generate(snapshot('LookupFixture.Box<System.String>', [
            request(BOX, 'LookupFixture.Box<System.String>', 'System.Int32'),
            request(BOX, 'LaterAlias', 'System.String')]))
        self.assertEqual(diagnostics, [])
        self.assertIn('Box_System_Int32JsonCodec', source)
        self.assertNotIn('Box_System_StringJsonCodec', source)

    def test_missing_construction_is_rejected(self):
        _, diagnostics = self.generate(snapshot('MissingAlias', [
            request('Other.Box`1', 'MissingAlias', 'System.String')]))
        self.assertIn('CXREST001', diagnostics)

    def test_repeated_annotations_are_preserved(self):
        _, diagnostics = self.generate(snapshot('System.String', [], [
            annotation('cxnet.System.Net.Http.Rest.Post', METHOD, ['route'], ['"/other"'])]))
        self.assertIn('CXREST004', diagnostics)

    def test_unrelated_generics_do_not_change_output(self):
        requests = [request(BOX, 'Alias', 'System.String')]
        baseline = self.generate(snapshot('Alias', requests))
        types, annotations = [], []
        for index in range(80):
            identity = 'Noise.Type' + str(index) + '`1'
            types.append(descriptor(identity, 'Type' + str(index) + '`1', 'Class', generic=True))
            annotations.append(annotation('System.Data.Serializable', identity))
            for argument in ['System.Int32', 'System.String', 'System.Boolean', 'System.Int64']:
                requests.append(request(identity, identity + '<' + argument + '>', argument))
        self.assertEqual(self.generate(snapshot('Alias', requests, annotations, types)), baseline)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    default = Path(__file__).resolve().parents[1] / 'generators/rest/.bin/Release/CxnetRestGenerator'
    if os.name == 'nt':
        default = default.with_suffix('.exe')
    parser.add_argument('--generator', type=Path, default=default)
    args, remaining = parser.parse_known_args()
    GENERATOR = args.generator.resolve()
    unittest.main(argv=[__file__, *remaining])
