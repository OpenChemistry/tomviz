# Installing development dependencies

```bash
cd <tomviz_repo>/acquisition
pip install requirements-dev.txt

```

# Starting the server

```bash
cd <tomviz_repo>/acquisition
python -m tomviz

```

This will start the server using a mock API for testing.


# Acquistion JSON-RPC interface

## Describe

This method returns a description of the parameters that a particular method
supports for the currently loaded adapter.

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "describe",
  "params": ['methodName']
}
```

### Response

Returns the parameter description.

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": [{...}]
}

```

## Connect

Connect to the instrument

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "connect",
  "params": {...}
}
```

### Response


```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": {...}
}

```

## Disconnect

Disconnect from the instrument

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "disconnect",
  "params": {...}
}
```

### Response


```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": {...}
}

## Setting tilt parameters

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "tilt_params",
  "params": {...}
}

```
### Reponse

Return the current title parameters

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": {...}
}

```

## acquisition_params

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "acquisition_params",
  "params": {...}
}

```
### Reponse

Return the current acquisition parameters

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": {...}
}

```

## Preview scan

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "preview_scan"
}

```
### Reponse

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": "<url>"
}

```
Where ```url``` is URL that can be used to fetch th 2D TIFF


## STEM accquire

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "stem_acquire"
}

```
### Reponse

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": "<url>"
}

```
Where ```url``` is URL that can be used to fetch th 2D TIFF
