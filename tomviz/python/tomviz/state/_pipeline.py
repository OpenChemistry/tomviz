from tomviz._wrapping import PipelineStateManagerBase

class PipelineStateManager(PipelineStateManagerBase):
    _instance = None

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = PipelineStateManagerBase.__new__(cls, *args, **kwargs)

        return cls._instance
