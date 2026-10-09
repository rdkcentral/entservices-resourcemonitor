-----------------
# ResourceMonitor

## Versions
`org.rdk.ResourceMonitor`

## Methods:
```
curl -X POST http://127.0.0.1:9998/jsonrpc -H "Content-Type: application/json" -d '{
    "jsonrpc":"2.0",
    "id":1,
    "method":"org.rdk.ResourceMonitor.GetMemInfo"
}'
 
curl -X POST http://127.0.0.1:9998/jsonrpc -H "Content-Type: application/json" -d '{
    "jsonrpc":"2.0",
    "id":2,
    "method":"org.rdk.ResourceMonitor.GetSwapUsed"
}'
 
3. GetPsiMetrics
curl -X POST http://127.0.0.1:9998/jsonrpc -H "Content-Type: application/json" -d '{
    "jsonrpc":"2.0",
    "id":3,
    "method":"org.rdk.ResourceMonitor.GetPsiMetrics",
    "params":{
        "metric":"full_avg60"
    }
}'
 
curl -X POST http://127.0.0.1:9998/jsonrpc -H "Content-Type: application/json" -d '{
    "jsonrpc":"2.0",
    "id":4,
    "method":"org.rdk.ResourceMonitor.GetPsiMetrics",
    "params":{
        "metric":"some_avg10"
    }
}'
 
curl -X POST http://127.0.0.1:9998/jsonrpc -H "Content-Type: application/json" -d '{
    "jsonrpc":"2.0",
    "id":5,
    "method":"org.rdk.ResourceMonitor.GetFlashSpace"
}'

```

## Responses
```
GetFlashSpace:
{"jsonrpc":"2.0","id":5,"result":{"total":314572800,"used":8484,"available":314564316}}

GetMemInfo:
{"jsonrpc":"2.0","id":1,"result":{"memAvailable":7493700,"swapFree":7674528,"swapTotal":7674528}}

GetSwapUsed:
{"jsonrpc":"2.0","id":2,"result":{"swapUsed":4096,"memUsedTotal":12288}}

GetPsiMetrics:
{"jsonrpc":"2.0","id":3,"result":0}


```

## Events
```
onReconciliationComplete(success/failure)
```