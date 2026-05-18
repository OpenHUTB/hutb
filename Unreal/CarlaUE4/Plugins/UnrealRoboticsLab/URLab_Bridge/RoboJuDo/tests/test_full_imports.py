import unittest


def test_registry(registry):
    """
    Test the module registry by attempting to import all registered modules.
    """
    flag_exception = False
    for type_name in registry.types:
        try:
            registry.get(type_name)
        except Exception as e:
            # Instead of failing the test, we log the error
            flag_exception = True
            print(f"Failed to import {type_name}: {e}")

    return flag_exception


class TestFullImports(unittest.TestCase):
    def test_import_configs(self):
        from robojudo.config import cfg_registry

        flag_exception = test_registry(cfg_registry)
        if flag_exception:
            self.fail("Some configs failed to import. Check the logs for details.")
        else:
            print("All configs imported successfully.")

    def test_import_controllers(self):
        from robojudo.controller import ctrl_registry

        flag_exception = test_registry(ctrl_registry)
        if flag_exception:
            self.fail("Some controllers failed to import. Check the logs for details.")
        else:
            print("All controllers imported successfully.")

    def test_import_environments(self):
        from robojudo.environment import env_registry

        flag_exception = test_registry(env_registry)
        if flag_exception:
            self.fail("Some environments failed to import. Check the logs for details.")
        else:
            print("All environments imported successfully.")

    def test_import_policies(self):
        from robojudo.policy import policy_registry

        flag_exception = test_registry(policy_registry)
        if flag_exception:
            self.fail("Some policies failed to import. Check the logs for details.")
        else:
            print("All policies imported successfully.")

    def test_import_pipelines(self):
        from robojudo.pipeline import pipeline_registry

        flag_exception = test_registry(pipeline_registry)
        if flag_exception:
            self.fail("Some pipelines failed to import. Check the logs for details.")
        else:
            print("All pipelines imported successfully.")
