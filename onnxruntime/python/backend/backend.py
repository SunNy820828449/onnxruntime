#-------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
#--------------------------------------------------------------------------
"""
Implements ONNX's backend API.
"""
from onnx.checker import check_model
from onnx.backend.base import Backend
from onnxruntime import InferenceSession, RunOptions, SessionOptions, get_device
from onnxruntime.backend.backend_rep import OnnxRuntimeBackendRep


class OnnxRuntimeBackend(Backend):
    """
    Implements ONNX's backend API with
    *onnxruntime*.
    """
    
    @classmethod
    def is_compatible(cls, model, device=None, **kwargs):
        """
        Return whether the model is compatible with the backend.
        
        :param model: unused
        :param device: None to use the default device or a string (ex: `'CPU'`)
        :return: boolean
        """
        if device is None:
            device = get_device()
        return cls.supports_device(device)

    @classmethod
    def supports_device(cls, device):
        """
        Check whether the backend is compiled with particular device support.
        In particular it's used in the testing suite.
        """
        return device in get_device()

    @classmethod
    def prepare(cls, model, device=None, **kwargs):
        """
        Load the model and creates a :class:`onnxruntime.InferenceSession`
        ready to be used as a backend.
        
        :param model: ModelProto (returned by `onnx.load`),
            string for a filename or bytes for a serialized model
        :param device: requested device for the computation,
            None means the default one which depends on
            the compilation settings
        :param kwargs: see :class:`onnxruntime.SessionOptions`
        :return: :class:`onnxruntime.InferenceSession`
        """
        if isinstance(model, OnnxRuntimeBackendRep):
            return model
        elif isinstance(model, InferenceSession):
            return OnnxRuntimeBackendRep(model)
        elif isinstance(model, (str, bytes)):
            options = SessionOptions()
            for k, v in kwargs.items():
                if hasattr(options, k):
                    setattr(options, k, v)
            inf = InferenceSession(model, options)
            if device is not None and not cls.supports_device(device):
                raise RuntimeError("Incompatible device expected '{0}', got '{1}'".format(device, get_device()))
            return cls.prepare(inf, device, **kwargs)
        else:
            # type: ModelProto
            check_model(model)
            bin = model.SerializeToString()
            return cls.prepare(bin, device, **kwargs)

    @classmethod
    def run_model(cls, model, inputs, device=None, **kwargs):
        """
        Compute the prediction.
        
        :param model: :class:`onnxruntime.InferenceSession` returned
            by :func:`onnxruntime.backend.prepare`
        :param inputs: inputs
        :param device: requested device for the computation,
            None means the default one which depends on
            the compilation settings
        :param kwargs: see :class:`onnxruntime.RunOptions`
        :return: predictions
        """
        rep = cls.prepare(model, device, **kwargs)
        options = RunOptions()
        for k, v in kwargs.items():
            if hasattr(options, k):
                setattr(options, k, v)
        return rep.run(inputs, options)

    @classmethod
    def run_node(cls, node, inputs, device=None, outputs_info=None, **kwargs):
        '''
        This method is not implemented as it is much more efficient
        to run a whole model than every node independently.
        '''
        raise NotImplementedError("It is much more efficient to run a whole model than every node independently.")


is_compatible = OnnxRuntimeBackend.is_compatible
prepare = OnnxRuntimeBackend.prepare
run = OnnxRuntimeBackend.run_model
supports_device = OnnxRuntimeBackend.supports_device
