# HoloOcean

![HoloOcean Image](client/docs/images/inspect_plane.jpg)

[![Documentation Status](https://readthedocs.org/projects/holoocean/badge/?version=latest)](https://holoocean.readthedocs.io/en/latest/?badge=latest)
 [![Build Status](https://robots.et.byu.edu:4000/api/badges/frostlab/holoocean/status.svg?ref=refs/heads/develop)](https://robots.et.byu.edu:4000/frostlab/holoocean)


HoloOcean is a high-fidelity simulator for underwater robotics built on top of Unreal Engine 4, and forked from Holodeck by BYU's PCCL Lab.

If you use HoloOcean for your research, please cite our ICRA publication;

```
@inproceedings{Potokar22icra,
  author = {E. Potokar and S. Ashford and M. Kaess and J. Mangelson},
  title = {Holo{O}cean: An Underwater Robotics Simulator},
  booktitle = {Proc. IEEE Intl. Conf. on Robotics and Automation, ICRA},
  address = {Philadelphia, PA, USA},
  month = may,
  year = {2022}
}
```

## Features
 - 3+ rich worlds with various infrastructure for generating data or testing underwater algorithms
 - Complete with common underwater sensors including DVL, IMU, optical camera, various sonar, depth sensor, and more
 - Highly and easily configurable sensors and missions
 - Multi-agent missions, including optical and acoustic communications
 - Novel sonar simulation framework for simulating imaging, profiling, sidescan, and echosounder sonars
 - Imaging sonar implementation includes realistic noise modeling for small sim-2-real gap
 - Easy installation and simple, OpenAI Gym-like Python interface
 - High performance - simulation speeds of up to 2x real time are possible. Performance penalty only for what you need
 - Run headless or watch your agents learn
 - Linux and Windows support

## Installation
`pip install holoocean`

(requires >= Python 3.6)

See [Installation](https://holoocean.readthedocs.io/en/latest/usage/installation.html) for complete instructions (including Docker).

## Documentation
* [Quickstart](https://holoocean.readthedocs.io/en/latest/usage/getting-started.html)
* [Changelog](https://holoocean.readthedocs.io/en/latest/changelog/changelog.html)
* [Examples](https://holoocean.readthedocs.io/en/latest/usage/getting-started.html#code-examples)
* [Agents](https://holoocean.readthedocs.io/en/latest/agents/agents.html)
* [Sensors](https://holoocean.readthedocs.io/en/latest/holoocean/sensors.html)
* [Available Packages and Worlds](https://holoocean.readthedocs.io/en/latest/packages/packages.html)
* [Docs](https://holoocean.readthedocs.io/en/latest/)

## Usage Overview
HoloOcean's interface is similar to [OpenAI's Gym](https://gym.openai.com/). 

We try and provide a batteries included approach to let you jump right into using HoloOcean, with minimal
fiddling required.

To demonstrate, here is a quick example using the `DefaultWorlds` package:

```python
import holoocean

# Load the environment. This environment contains a hovering AUV in a pier
env = holoocean.make("PierHarbor-Hovering")

# You must call `.reset()` on a newly created environment before ticking/stepping it
env.reset()                         

# The AUV takes commands for each thruster
command = [0, 0, 0, 0, 10, 10, 10, 10]   

for i in range(30):
    state = env.step(command)  
```

- `state`: dict of sensor name to the sensor's value (nparray).

If you want to access the data of a specific sensor, import sensors and
retrieving the correct value from the state dictionary:

```python
print(state["DVLSensor"])
```

## Multi Agent-Environments
HoloOcean supports multi-agent environments.

Calls to [`step`](https://holoocean.readthedocs.io/en/latest/holoocean/environments.html#holoocean.environments.HoloOceanEnvironment.step) only provide an action for the main agent, and then tick the simulation. 

[`act`](https://holoocean.readthedocs.io/en/latest/holoocean/environments.html#holoocean.environments.HoloOceanEnvironment.act) provides a persistent action for a specific agent, and does not tick the simulation. After an 
action has been provided, [`tick`](https://holoocean.readthedocs.io/en/latest/holoocean/environments.html#holoocean.environments.HoloOceanEnvironment.tick) will advance the simulation forward. The action is persisted until another call to `act` provides a different action.

```python
import holoocean
import numpy as np

env = holoocean.make("Dam-Hovering")
env.reset()

# Provide an action for each agent
env.act('auv0', np.array([0, 0, 0, 0, 10, 10, 10, 10]))

# Advance the simulation
for i in range(300):
  # The action provided above is repeated
  states = env.tick()
```

You can access the sensor states as before:

```python
dvl = states["auv0"]["DVLSensor"]
location = states["auv0"]["DepthSensor"]
```

(`auv0` comes from the [scenario configuration file](https://holoocean.readthedocs.io/en/latest/packages/docs/scenarios.html))

## Running HoloOcean Headless
HoloOcean can run headless with GPU accelerated rendering. See [Using HoloOcean Headless](https://holoocean.readthedocs.io/en/latest/usage/running-headless.html)

