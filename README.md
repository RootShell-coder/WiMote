# WiMote_IR IR2MQTT/MQTT2IR

Remote Control with Maximum Features

![alt text](<screen/IR Remote Control.png>)

[mqtt schema](docs/mqtt.json)

[api swagger](docs/api.yml)

## mqtt

send IR command `<user>/<clientID>/ir/transmitted/set`

```json
{
  "protocol": "NEC",
  "value": "0x20DFE01F",
  "bits": 32
}
```
