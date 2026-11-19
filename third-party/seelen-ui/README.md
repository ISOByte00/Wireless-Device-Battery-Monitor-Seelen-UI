# Seelen UI integration

`make SEELEN_INTEGRATION=1` Seelen destekli BatteryHub derlemesini oluşturur. Bu derleme, BatteryHub'ın mevcut cihaz taramasından gelen veriyi `127.0.0.1:37842/seelen/state-text` üzerinden Fancy Toolbar'a verir; ikinci bir donanım taraması yapmaz.

Plugin kaynağı `plugin/metadata.yml` ve `plugin/template.js` dosyalarıdır. Dağıtım dosyasını üretmek veya doğrulamak için:

```text
python third-party/seelen-ui/scripts/sync_plugin.py
python third-party/seelen-ui/scripts/sync_plugin.py --check
```

`BatteryHub.yaml` Seelen UI'den içe aktarılır. Plugin başka toolbar modüllerini veya Seelen `state.yml` dosyasını değiştirmez. Seelen'in eski HTTP verisini koruması plugin API davranışıdır; bu entegrasyon buna karşı bir watchdog çalıştırmaz.
