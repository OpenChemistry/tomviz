import pytest

from tomviz_acquisition.jsonrpc import JsonRpcHandler, jsonrpc_message


@pytest.fixture
def handler():
    return JsonRpcHandler('/some/path')


def test_invalid_request(handler):
    request = {}
    response = handler.rpc(request)
    excepted = jsonrpc_message({
        'id': None,
        'error': {
            'message': 'Invalid request.',
            'code': -32600
        }
    })
    assert response == excepted

    id = 1234
    request = {
        'id': id
    }
    response = handler.rpc(request)
    excepted = jsonrpc_message({
        'id': id,
        'error': {
            'message': 'Invalid request.',
            'code': -32600
        }
    })
    assert response == excepted

    request = {
        'id': id,
        'method': 'foo'
    }
    response = handler.rpc(request)
    excepted = jsonrpc_message({
        'id': id,
        'error': {
            'message': 'Invalid request.',
            'code': -32600
        }
    })
    assert response == excepted


def test_method_not_found(handler):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'foo'
    })
    response = handler.rpc(request)
    expected = jsonrpc_message({
        'id': id,
        'error': {
            'message': 'Method "foo" not found.',
            'code': -32601,
            'data': 'foo'
        }
    })

    assert response == expected


def test_method_no_params(handler):

    def test():
        return {
            'data': 'big data!'
        }

    handler.add_method('test', test)

    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'test'
    })
    response = handler.rpc(request)
    expected = jsonrpc_message({
        'id': id,
        'result': {
            'data': 'big data!'
        }
    })

    assert response == expected


def test_method_dict_params(handler):
    message = 'Hey!'

    def test(message=None):
        return {
            'data': message
        }

    handler.add_method('test', test)

    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'test',
        'params': {
            'message': message
        }
    })
    response = handler.rpc(request)
    expected = jsonrpc_message({
        'id': id,
        'result': {
            'data': message
        }
    })

    assert response == expected


def test_method_list_params(handler):
    message = 'Hey!'

    def test(message):
        return {
            'data': message
        }

    handler.add_method('test', test)

    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'test',
        'params': [message]
    })
    response = handler.rpc(request)
    expected = jsonrpc_message({
        'id': id,
        'result': {
            'data': message
        }
    })

    assert response == expected
