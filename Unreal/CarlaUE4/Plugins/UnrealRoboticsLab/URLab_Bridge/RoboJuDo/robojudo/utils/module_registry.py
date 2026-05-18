import importlib


class Registry:
    """
    Module registry for dynamic loading and registration of classes.
    """

    def __init__(self, package: str, base_class: type):
        self.PACKAGE = package
        self.BASE_CLASS = base_class
        self.modules: dict[str, str] = {}
        self.registered_modules: dict[str, type] = {}

    @property
    def types(self) -> list[str]:
        """
        Get a list of all module types.
        """
        return list(self.modules.keys()) + list(self.registered_modules.keys())

    def add(self, type_name: str, module_path: str):
        """
        Add a new module to the registry.
        """
        if type_name in self.modules:
            raise ValueError(f"Type {type_name} already registered with module {self.modules[type_name]}")
        self.modules[type_name] = module_path

    def register(self, cls: type) -> type:
        """
        Register a new module.
        """
        if not issubclass(cls, self.BASE_CLASS):
            raise ValueError(f"Controller must be a subclass of {self.BASE_CLASS.__name__}")
        self.registered_modules[cls.__name__] = cls
        return cls

    def get(self, type_name: str) -> type:
        """
        Get a module class by its type.
        """
        if type_name not in self.registered_modules:
            if type_name not in self.modules:
                raise NotImplementedError(f"Unknown type: {type_name}. Available: {self.types}")

            module_path = self.modules[type_name]
            try:
                importlib.import_module(name=module_path, package=self.PACKAGE)
            except ImportError as e:
                print(e)
                raise RuntimeError(f"Failed to import module for type {type_name}: {module_path}") from e

            if type_name not in self.registered_modules:
                raise RuntimeError(f"{self.PACKAGE}.{type_name} not registered after importing {module_path}")

            print(f"[Registry][{self.PACKAGE}] {type_name}, total: {len(self.registered_modules)}")
        return self.registered_modules[type_name]
