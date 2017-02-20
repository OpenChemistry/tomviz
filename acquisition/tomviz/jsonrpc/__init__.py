import json
import traceback
import bottle
from bottle import post

endpoint_map = {}

# Invalid JSON was received by the server. An error occurred on the server
# while parsing the JSON text.
PARSE_ERROR = -32700
# The JSON sent is not a valid Request object.
INVALID_REQUEST = -32600
# The method does not exist / is not available.
METHOD_NOT_FOUND = -32601
# Invalid method parameter(s).
INVALID_PARAMS = -32602
# Internal JSON-RPC error.
INTERNAL_ERROR = -32603
# -32000 to -32099 Server error
SERVER_ERROR = -32000

JSONRPC_VERSION = '2.0'


def jsonrpc_message(message):
    message['jsonrpc'] = JSONRPC_VERSION

    return message


class JsonRpcError(Exception):
    code = -32000
    message = None

    def __init__(self, message=None, data=None):
        if message is not None:
            self.message = message
        self.data = data

    def to_json(self):
        error = {
            'code': self.code,
            'message': self.message,
        }

        if self.data:
            error['data'] = self.data

        return error


class ParseError(JsonRpcError):
    code = PARSE_ERROR
    message = "Parse Error."


class InvalidRequest(JsonRpcError):
    code = INVALID_REQUEST
    message = "Invalid Request."


class MethodNotFound(JsonRpcError):
    code = METHOD_NOT_FOUND
    message = "Method not found."


class InvalidParams(JsonRpcError):
    code = INVALID_PARAMS
    message = "Invalid params."


class InternalError(JsonRpcError):
    code = INTERNAL_ERROR
    message = "Internal Error."


class ServerError(JsonRpcError):
    code = SERVER_ERROR
    message = "Server Error."


class JsonRpcHandler(object):
    def __init__(self, path):
        self._path = path
        self._methods = {}

    def rpc(self, request):

        jsonrpc = request.get('jsonrpc')
        id = request.get('id')
        method = request.get('method')
        params = request.get('params', {})

        try:
            if jsonrpc != JSONRPC_VERSION or not id or not method:
                raise InvalidRequest()

            if method not in self._methods:
                msg = 'Method "%s" not found.' % method
                raise MethodNotFound(message=msg, data=method)

            func = self._methods[method]

            try:
                if isinstance(params, list):
                    result = func(*params)
                elif isinstance(params, dict):
                    result = func(**params)
                else:
                    raise InvalidParams()
            except Exception as ex:
                raise ServerError(message=ex.message,
                                  data=json.dumps(traceback.format_exc()))

            return self._response(id, result)
        except JsonRpcError as err:
            return jsonrpc_message({
                'id': id,
                'error': err.to_json()
            })


    def _response(self, id, result):
        response = {
            'jsonrpc': JSONRPC_VERSION,
            'id': id,
            'result': result
        }

        return response

    def add_method(self, name,  method):
        self._methods[name] = method


jsonrpc_code_to_status = {
    -32700: 500,
    -32600: 400,
    -32601: 404,
    -32602: 500,
    -32603: 500
}


class endpoint(object):
    def __init__(self, path):
        if path not in endpoint_map:
            self._endpoint = JsonRpcHandler(path)

            # register with bottle
            def handle_rpc():
                json_body = {}
                try:
                    if bottle.request.headers['CONTENT_TYPE'] \
                       not in ('application/json', 'application/json-rpc'):
                        raise ParseError(
                            message='Invalid content type.',
                            data=bottle.request.headers['CONTENT_TYPE'])
                    try:
                        json_body = bottle.json_loads(
                            bottle.request.body.read())
                    except ValueError as err:
                        raise ParseError(
                            message='Invalid JSON.',
                            data=json.dumps(traceback.format_exc()))

                    response = self._endpoint.rpc(json_body)
                    # If we have error set the HTTP status code
                    if 'error' in response:
                        error = response['error']
                        status = jsonrpc_code_to_status.get(error['code'], 500)
                        bottle.response.status = status

                    return response
                except JsonRpcError as err:
                    return jsonrpc_message({
                        'id': json_body.get('id'),
                        'error': err.to_json()
                    })

            post(path, callback=handle_rpc)
            endpoint_map[path] = self._endpoint
        else:
            self._endpoint = endpoint_map[path]

    def __call__(self, func):
        self._endpoint.add_method(func.__name__, func)
