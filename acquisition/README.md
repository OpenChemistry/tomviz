# Acquistion JSON-RPC interface

## Setting tilt angle

### Request

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "method": "set_tilt_angle",
  "params": [<angle>]
}

```
### Reponse

```json
{
  "jsonrpc": "2.0",
  "id": "<id>",
  "result": "<angle>"
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
