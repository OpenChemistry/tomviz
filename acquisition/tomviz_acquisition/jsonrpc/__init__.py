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
    def __init__(self, code, message, data=None):
        self.code = code
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
    def __init__(self, message='Parse error.', data=None):
        super(ParseError, self).__init__(PARSE_ERROR, message, data)


class InvalidRequest(JsonRpcError):
    def __init__(self, message='Invalid request.', data=None):
        super(InvalidRequest, self).__init__(INVALID_REQUEST, message, data)


class MethodNotFound(JsonRpcError):
    def __init__(self, message='Method not found.', data=None):
        super(MethodNotFound, self).__init__(METHOD_NOT_FOUND, message, data)


class InvalidParams(JsonRpcError):
    def __init__(self, message='Invalid parameters.', data=None):
        super(InvalidParams, self).__init__(INVALID_PARAMS, message, data)


class InternalError(JsonRpcError):
    def __init__(self, message='Internal error.', data=None):
        super(InternalError, self).__init__(INTERNAL_ERROR, message, data)


class ServerError(JsonRpcError):
    def __init__(self, message='Server error.', data=None):
        super(ServerError, self).__init__(SERVER_ERROR, message, data)


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
            if jsonrpc != JSONRPC_VERSION or id is None or method is None:
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
            except ValueError as vex:
                raise InvalidParams(message=str(vex),
                                    data=json.dumps(traceback.format_exc()))
            except Exception as ex:
                raise ServerError(message=str(ex),
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
                    except ValueError:
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

            # In bottle /path/ and /path are two different routes, because of
            # http://www.ietf.org/rfc/rfc3986.txt, so register them both
            if path[-1] != '/' and path[-1] != '>':
                post('%s/' % path, callback=handle_rpc)

            endpoint_map[path] = self._endpoint
        else:
            self._endpoint = endpoint_map[path]

    def __call__(self, func):
        self._endpoint.add_method(func.__name__, func)
