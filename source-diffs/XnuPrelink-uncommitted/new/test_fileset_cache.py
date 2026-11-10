import tempfile
import unittest
from pathlib import Path
from fileset_cache import AnalysisCache, analysis_signature

class AnalysisCacheTests(unittest.TestCase):
    def test_warm_cache_avoids_builder(self):
        with tempfile.TemporaryDirectory() as d:
            c=AnalysisCache(d, 'decoder-v1')
            self.assertEqual(c.load({'sha256':'a','header':32}, lambda: {'value':17}), {'value':17})
            warm=AnalysisCache(d, 'decoder-v1')
            self.assertEqual(warm.load({'header':32,'sha256':'a'}, lambda: self.fail('decoded again')), {'value':17})
            self.assertEqual((c.misses,warm.hits), (1,1))

    def test_input_header_and_code_invalidate(self):
        with tempfile.TemporaryDirectory() as d:
            base={'sha256':'a','header':32,'fixup_header':0}
            AnalysisCache(d,'v1').load(base,lambda:1)
            for identity,version in [(dict(base,sha256='b'),'v1'),(dict(base,header=64),'v1'),(dict(base,fixup_header=32),'v1'),(base,'v2')]:
                c=AnalysisCache(d,version)
                self.assertEqual(c.load(identity,lambda:2),2)
                self.assertEqual(c.misses,1)

    def test_corrupt_cache_rebuilt(self):
        with tempfile.TemporaryDirectory() as d:
            c=AnalysisCache(d,'v1'); c.load({'sha256':'a'},lambda:1)
            next(Path(d).glob('*.pickle')).write_bytes(b'broken')
            self.assertEqual(c.load({'sha256':'a'},lambda:2),2)
            self.assertFalse(list(Path(d).glob('*.tmp')))

    def test_failed_builder_does_not_publish(self):
        with tempfile.TemporaryDirectory() as d:
            def fail(): raise ValueError('decode failed')
            with self.assertRaises(ValueError):AnalysisCache(d,'v1').load({},fail)
            self.assertFalse(list(Path(d).glob('*')))

    def test_signature_excludes_binding_fixtures(self):
        from unittest.mock import patch
        real=Path.read_bytes; reads=[]
        def read(path):reads.append(path.name); return real(path)
        with patch.object(Path,'read_bytes',read):self.assertEqual(len(analysis_signature()),64)
        self.assertEqual(set(reads),{'fileset_cache.py','fileset_match.py','kc_image.py','fileset_shape.py'})
