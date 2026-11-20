# Seelen UI integration

`make SEELEN_INTEGRATION=1` Seelen destekli BatteryHub derlemesini oluşturur. Bu derleme, BatteryHub'ın mevcut cihaz taramasından gelen veriyi `127.0.0.1:37842/seelen/state-text` üzerinden Fancy Toolbar'a verir; ikinci bir donanım taraması yapmaz.

Plugin kaynağı `plugin/metadata.yml` ve `plugin/template.js` dosyalarıdır. Dağıtım dosyasını üretmek veya doğrulamak için:

```text
python third-party/seelen-ui/scripts/sync_plugin.py
python third-party/seelen-ui/scripts/sync_plugin.py --check
```

`BatteryHub-Seelen.yml` Seelen UI'ye kurulan tek, bağımsız plugin dosyasıdır. Plugin başka toolbar modüllerini veya Seelen `state.yml` dosyasını değiştirmez. Bridge cihaz tarama zamanından ayrı bir heartbeat yayımlar; plugin, Seelen'in başarısız HTTP isteğinde eski cevabı saklamasına karşı reaktif Date scope ile bu heartbeat'in yaşını denetler.
