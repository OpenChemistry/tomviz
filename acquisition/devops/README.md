# Remote access to server logs

```bash
watch -n1 curl "http://localhost:8080/log/<log>?bytes=3000"
```
Where ```<log>``` is one of:

- ```stderr``` - Redirected stderr ( server must be started with ```-r``` options ). Bottle writes it output here.
- ```stdout``` - Redirected stdout ( server must be started with ```-r``` options ).
- ```debug``` - The servers debug log.


# Deploying a source adapter

If the server is started in dev model a source adapter can be deployed remotely
using a JSON-RPC method.

The server can be started in development mode using the following command:

```bash
python -m tomviz --dev

```

The following [python script](https://raw.githubusercontent.com/OpenChemistry/tomviz/master/acquisition/devops/deploy_adapter.py)
can be using to deploy an adapter:

```bash
python deploy_adapter.py -a <adapter_class_name> -p <path_to_source_file>

```



