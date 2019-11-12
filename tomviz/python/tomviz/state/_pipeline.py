from tomviz._wrapping import PipelineStateManagerBase

class PipelineStateManager(PipelineStateManagerBase):
    _instance = None

    # Need to define a constructor as the implementation on the C++ side is
    # static.
    def __init__(self):
        pass

    def __call__(cls):
        if cls._instance is None:
            cls._instance = super(PipelineStateManager, cls).__call__()

        return cls._instances