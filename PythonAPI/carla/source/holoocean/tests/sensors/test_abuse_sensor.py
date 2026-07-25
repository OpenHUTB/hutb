def test_abuse_uav(abuse_world):
    """Test that the UAV's blades hitting the ground triggers the abuse sensor"""
    # Collide the uav's blades with the ground and check for abuse.
    abuse_world.agents["uav0"].teleport([0, 0, 1], [0, 180, 0])
    abused = False

    for _ in range(20):
        if abuse_world.tick()["uav0"]["AbuseSensor"] == 1:
            abused = True

    assert abused, "The abuse sensor didn't trigger from uav blade collision!"


def test_abuse_turtle(abuse_world):
    """Test that the turtle is abused when it is flipped over"""

    # Flip the turtle and check if it's abused
    abuse_world.agents["turtle0"].teleport([0, 0, 1], [0, 180, 0])
    abuse_world.tick(20)
    assert (
        abuse_world.tick()["turtle0"]["AbuseSensor"] == 1
    ), "The abuse sensor didn't trigger from the turtle agent flipping over"


def test_abuse_falling(agent_abuse_world):
    """Test that the uav, turtle, and android are abused when they are dropped from a height"""
    agent, abuse_world = agent_abuse_world

    abused = False
    for _ in range(100):
        if abuse_world.tick()[agent]["AbuseSensor"] == 1:
            abused = True

    assert abused, "Agent {} was not abused!".format(agent)
