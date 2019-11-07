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

    expected_sha = ('ae828cfdabffe364bc46d3c0229d3f1bfde4b9157bb3db0049803d8'
                    '9ded713effb752ea2442e386cea6aabbfed85878f6fdd04ecc14ac8'
                    '61f56ee4361b51efe7')

    # assert that we have the right output
    assert sha.hexdigest() == expected_sha
