from hashlib import sha512
import h5py

from tomviz.cli import main
from click.testing import CliRunner


def test_external_pipeline(test_state_file, tmpdir):
    output_path = tmpdir.join('output.emd')
    runner = CliRunner()
    result = runner.invoke(main, ['-s', test_state_file.strpath,
                                  '-o', output_path.strpath])
    assert result.exit_code == 0

    sha = sha512()
    with h5py.File(output_path.strpath, 'r') as f:
        tomography = f['data/tomography']
        sha.update(tomography['data'][:])

    excepted_sha = ('c9cc816ece3c32eda0b394a8a9242bbeb128e8ee7fa380d5aa731ab'
                    'a8e2cc4b889ad508be94089c8961eeb97316b05855fe8f7df29c8d4'
                    'ec496168afe3775955')

    # assert that we have the right output
    assert sha.hexdigest() == excepted_sha
