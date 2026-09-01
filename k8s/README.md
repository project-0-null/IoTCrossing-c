# Manifestos Kubernetes

Descricao dos manifestos para deploy da infraestrutura no cluster:

- `mosquitto.yaml`: Deploy do broker MQTT (Eclipse Mosquitto) no namespace `fiware-kata` com NodePort na porta 31883.
- `iota-ul.yaml`: Deploy do FIWARE IoT Agent UltraLight no namespace `fiware-kata`, conectado internamente ao Mosquitto, Orion e MongoDB.
- `tailscale.yaml`: Deploy do gateway Tailscale com proxies socat para expor as portas do cluster (MQTT 1883, IoT Agent 4041, Orion 1026/31330, Grafana 3000, Prometheus 9090 e SSH 22).

## Como aplicar

```bash
kubectl apply -f k8s/mosquitto.yaml
kubectl apply -f k8s/iota-ul.yaml
kubectl apply -f k8s/tailscale.yaml
```
