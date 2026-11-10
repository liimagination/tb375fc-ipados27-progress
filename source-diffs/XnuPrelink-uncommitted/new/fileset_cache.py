"""Local trusted analysis cache; never a substitute for exact-input map validation."""
import hashlib
import json
import os
from pathlib import Path
import pickle
import sys
import tempfile


def analysis_signature():
    root = Path(__file__).parent
    digest = hashlib.sha256()
    # Reviewed bindings affect matching, not decoding; intentionally excluded.
    for name in ('fileset_cache.py', 'fileset_match.py', 'kc_image.py', 'fileset_shape.py'):
        digest.update(name.encode() + b"\0" + (root / name).read_bytes())
    digest.update(str(sys.version_info[:2]).encode())
    return digest.hexdigest()


class AnalysisCache:
    """Pickles are read only from an explicitly selected, trusted local directory."""

    def __init__(self, directory, signature=None):
        self.directory = Path(directory)
        self.signature = signature or analysis_signature()
        self.hits = self.misses = 0

    def load(self, identity, build):
        key = hashlib.sha256(json.dumps(dict(schema=1, code=self.signature, input=identity), sort_keys=True).encode()).hexdigest()
        path = self.directory / (key + '.pickle')
        try:
            with path.open('rb') as stream:
                result = pickle.load(stream)
            self.hits += 1
            return result
        except (OSError, EOFError, ValueError, pickle.PickleError, AttributeError, ImportError):
            pass
        self.misses += 1
        result = build()
        self.directory.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(prefix=key+'.', suffix='.tmp', dir=self.directory)
        try:
            with os.fdopen(descriptor, 'wb') as stream:
                pickle.dump(result, stream, protocol=pickle.HIGHEST_PROTOCOL)
            os.replace(temporary, path)
        finally:
            Path(temporary).unlink(missing_ok=True)
        return result
