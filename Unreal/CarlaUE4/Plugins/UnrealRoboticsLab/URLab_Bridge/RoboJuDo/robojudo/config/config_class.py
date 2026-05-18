from pydantic import BaseModel


class Config(BaseModel):
    def to_dict(self) -> dict:
        return self.model_dump()
