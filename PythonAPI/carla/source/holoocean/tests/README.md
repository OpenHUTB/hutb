# HoloOcean Integration Tests

## Pre-reqs

`pip install tox`

### What is tox
Tox automatically creates a virtualenv with the dependencies specified in
`config.py` and runs `pytest` against that virtualenv. This allows us to
test holoocean like it were a package installed on a fresh machine.

## I just want to run tests against the code I'm working with

### Install your development copy of holoocean
You can have pip install the `holoocean` module in "editable" mode - meaning
you can `import holoocean` anywhere, and it will use the code in
`src/holoocean`.

First, remove whatever version of holoocean you have installed
`pip uninstall holoocean`

Then, from the root of this repo run
`pip install --editable .`

Open a `python` terminal and make sure when you `import holoocean` and run
`holoocean.util.get_holoocean_version()` it prints `"X.Y.Zdev"`

### Run pytest

Once you have installed your dev copy of holoocean, run
`pytest` from the root of this repo. You should see output like this
```
============================= test session starts =============================
platform win32 -- Python 3.7.1, pytest-4.5.0, py-1.8.0, pluggy-0.12.0
cachedir: .tox\py37\.pytest_cache
rootdir: C:\Users\jayde\Documents\holoocean, inifile: pytest.ini
collected 28 items

tests\scenarios\test_loading_scenarios.py ..............           [ 50%]
tests\scenarios\test_reset.py .......                              [ 75%]
tests\scenarios\test_rgb_camera_not_null.py .......                [100%]

========================= 28 passed in 131.77 seconds =========================
___________________________________ summary ___________________________________
  py37: commands succeeded
  congratulations :)
```

In Pycharm, you can also right click on a test and run/debug it individually

## Run Tox
Just type `tox` from the root of this repo.