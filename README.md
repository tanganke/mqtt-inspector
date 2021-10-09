# mqtt-inspector

simple Linux program inspect mqtt topic.

example usage:

```shell
mqtt-inspector --verbose ${ADDRESS} ${TOPIC}
```

which is equivalent to

```shell
mosquitto_sub -h ${ADDRESS} ${TOPIC}
```
