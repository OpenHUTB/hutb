import holoocean
import pytest
import uuid

from scipy.stats import multivariate_normal as mvn
import numpy as np

eps = 1e-9


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test_mvn",
        "world": "TestWorld",
        "main_agent": "turtle0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "turtle0",
                "agent_type": "TurtleAgent",
                "sensors": [
                    {
                        "sensor_type": "LocationSensor",
                        "sensor_name": "noise",
                        "configuration": {},
                    },
                    {"sensor_type": "LocationSensor", "sensor_name": "clean"},
                ],
                "control_scheme": 0,
                "location": [-1.5, -1.50, 3.0],
            }
        ],
    }
    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
    with holoocean.environments.HoloOceanEnvironment(
        scenario=scenario,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
        ticks_per_sec=30,
    ) as env:
        yield env


@pytest.mark.parametrize("num", range(5))
def test_sigma_scalar(env, num):
    """Make sure it can take a plain std as a parameter"""
    # Generate a sigma
    sigma = np.random.rand() * 10
    cov = np.diag([sigma, sigma, sigma]) ** 2

    env._scenario["agents"][0]["sensors"][0]["configuration"].pop("Cov", None)
    env._scenario["agents"][0]["sensors"][0]["configuration"]["Sigma"] = sigma

    env.reset()

    for i in range(5):
        state = env.tick()

        p = mvn.pdf(state["noise"], mean=state["clean"], cov=cov)
        assert p > eps


@pytest.mark.parametrize("num", range(5))
def test_sigma_vector(env, num):
    """Make sure it can take diagonal array as std"""
    # Generate a sigma
    sigma = np.random.rand(3) * 10
    cov = np.diag(sigma**2)

    env._scenario["agents"][0]["sensors"][0]["configuration"].pop("Cov", None)
    env._scenario["agents"][0]["sensors"][0]["configuration"]["Sigma"] = list(sigma)

    env.reset()

    for i in range(5):
        state = env.tick()

        p = mvn.pdf(state["noise"], mean=state["clean"], cov=cov)
        assert p > eps


@pytest.mark.parametrize("num", range(5))
def test_cov_num(env, num):
    """Make sure it can take covariance as a scalar"""
    # Generate a sigma
    cov_num = np.random.rand() * 10
    cov = np.diag([cov_num, cov_num, cov_num])

    env._scenario["agents"][0]["sensors"][0]["configuration"].pop("Sigma", None)
    env._scenario["agents"][0]["sensors"][0]["configuration"]["Cov"] = cov_num

    env.reset()

    for i in range(5):
        state = env.tick()

        p = mvn.pdf(state["noise"], mean=state["clean"], cov=cov)
        assert p > eps


@pytest.mark.parametrize("num", range(5))
def test_cov_vector(env, num):
    """Make sure it can take diagonal array as cov"""
    # Generate a cov
    cov_array = np.random.rand(3) * 10
    cov = np.diag(cov_array)

    env._scenario["agents"][0]["sensors"][0]["configuration"].pop("Sigma", None)
    env._scenario["agents"][0]["sensors"][0]["configuration"]["Cov"] = list(cov_array)

    env.reset()

    for i in range(5):
        state = env.tick()

        p = mvn.pdf(state["noise"], mean=state["clean"], cov=cov)
        assert p > eps


@pytest.mark.parametrize("num", range(5))
def test_cov_matrix(env, num):
    """Make sure it can take a full matrix as covariance"""
    # Generate a cov
    cov = np.random.rand(3, 3) * 10
    cov = cov @ cov.T

    env._scenario["agents"][0]["sensors"][0]["configuration"].pop("Sigma", None)
    env._scenario["agents"][0]["sensors"][0]["configuration"]["Cov"] = cov.tolist()

    env.reset()

    for i in range(5):
        state = env.tick()

        p = mvn.pdf(state["noise"], mean=state["clean"], cov=cov)
        assert p > eps
