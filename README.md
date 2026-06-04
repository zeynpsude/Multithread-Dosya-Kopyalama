# Çok Threadli Dosya Kopyalama ve Tek Yönlü Senkronizasyon Aracı

C ile yazılmış, POSIX threads (pthreads) tabanlı bir komut satırı aracıdır. Kaynak dizindeki eksik veya değişmiş dosyaları hedef dizine tek yönlü olarak kopyalar/günceller. Kaynak dizinde silinen dosyalar hedef dizinden silinmez; bu yüzden proje "tam ayna senkronizasyonu" değil, tek yönlü kopyalama ve güncelleme mantığıyla çalışır.

Temel akış şudur: scanner kaynak dizini dolaşır, kopyalanması gereken dosyaları thread-safe bir kuyruğa ekler; worker thread'ler bu kuyruktan iş alıp dosyaları paralel şekilde hedef dizine yazar.

## Derleme

```bash
make
```

Bağımlılıklar:

- `gcc`
- `make`
- POSIX threads desteği (`pthread`)
- Linux / Unix benzeri çalışma ortamı

## Kullanım

```bash
./copy_tool <thread_sayisi> <kaynak_dizin> <hedef_dizin>
```

Örnek:

```bash
./copy_tool 4 ./source_dir ./dest_dir
```

Tüm işlemler, çalıştırdığın dizindeki `copy_tool.log` dosyasına timestamp ve thread ID ile birlikte yazılır.

## Son Durum

Projenin mevcut halinde:

- `main.c`, argüman kontrolünü yapar, hedef ana dizini oluşturur, worker thread'leri başlatır ve işlem sonunda kuyruğu kapatır.
- `scanner.c`, kaynak dizini recursive olarak tarar. Alt dizinleri gezmek için ayrı bir dizin kuyruğu (`DirQueue`) kullanır.
- `scanner.c`, hedefte olmayan dosyaları `COPY`, hedefte olup kaynakta daha yeni veya farklı boyutta olan dosyaları `UPDATE` işi olarak kuyruğa ekler.
- `queue.c`, dosya işleri için sabit kapasiteli (`QUEUE_SIZE = 512`) thread-safe FIFO kuyruk sağlar.
- `worker.c`, kuyruktan aldığı dosyaları 4096 baytlık bloklarla okur/yazar.
- `log.c`, log dosyasına yazma işlemini mutex ile koruyarak thread-safe loglama yapar.

Bu yapıyı günlük hayattan şöyle düşünebilirsin: scanner depoda gezip taşınacak kutuların listesini çıkaran kişi, worker thread'ler ise listedeki kutuları paralel taşıyan işçiler gibidir. Kuyruk da herkesin sırayla baktığı ortak iş listesidir.

## Mimari

Üretici-tüketici (producer-consumer) deseni üzerine kurulmuştur.

| Modül       | Görev |
|-------------|-------|
| `main.c`    | Argüman parse, thread oluşturma, kuyruğun yaşam döngüsü |
| `scanner.c` | Kaynak dizini recursive tarar, iş öğelerini kuyruğa ekler (producer) |
| `queue.c`   | Mutex + condition variable ile korunmuş thread-safe FIFO kuyruk ve dizin kuyruğu |
| `worker.c`  | Kuyruktan iş alır, dosyayı blok blok kopyalar (consumer) |
| `log.c`     | Thread-safe loglama (mutex korumalı) |

Senkronizasyon:

- `pthread_mutex_t` ile karşılıklı dışlama sağlanır.
- `pthread_cond_t` (`not_empty`, `not_full`) ile worker thread'lerin boş kuyrukta beklemesi ve kuyruk dolduğunda scanner'ın beklemesi sağlanır.
- İşlem bittiğinde `shutdown` bayrağı ayarlanır ve bekleyen worker thread'ler uyandırılır.

## Özellikler

- Recursive dizin tarama (alt dizinler dahil)
- Hedefte eksik olan dosyaların otomatik kopyalanması
- Kaynak dosyanın `mtime` değeri hedeftekinden yeniyse dosyanın güncellenmesi
- Kaynak ve hedef dosya boyutu farklıysa dosyanın güncellenmesi
- Hedef alt dizinlerin gerektiğinde oluşturulması
- Sembolik linklerin (`symlink`) atlanması
- Büyük dosyalar için blok bazlı okuma/yazma (`BUFFER_SIZE = 4096`)
- Sabit kapasiteli dosya iş kuyruğu (`QUEUE_SIZE = 512`)
- Tüm işlemler için detaylı log (`copy_tool.log`)
- Yapılandırılabilir worker thread sayısı

## Sınırlamalar

- Kaynak dizinde silinen dosyalar hedef dizinden silinmez.
- Hedef dosyalar `0644` izniyle oluşturulur; kaynak dosya izinleri birebir korunmaz.
- Sembolik linkler kopyalanmaz, bilinçli olarak atlanır.
- Dizin izinleri oluşturulurken kaynak dizinin izinleri temel alınır, ancak tüm metadata birebir korunmaz.
- Dosya sahipliği, grup bilgisi, access time (`atime`) gibi metadata alanları senkronize edilmez.
- Bazı tarama hatalarında işlem durdurulmaz; hata loglanır veya ilgili öğe atlanır, ardından tarama devam eder.
- Bu araç iki yönlü senkronizasyon yapmaz; yalnızca kaynak dizinden hedef dizine doğru çalışır.

## Log Formatı

```text
[2025-05-16 14:23:01] [tid:140234567892480] [COPY]   ./kaynak/a.txt -> ./hedef/a.txt (1024 bayt)
[2025-05-16 14:23:01] [tid:140234567892481] [UPDATE] ./kaynak/b.log yenilendi (2048 bayt)
[2025-05-16 14:23:01] [tid:140234567892482] [ERROR]  Kaynak dosya acilamadi: ./kaynak/locked.bin
```

Not: `COPY` loglarında kaynak ve hedef yol birlikte yazılır. `UPDATE` loglarında mevcut implementasyon hedef dosya yolunu ve yazılan byte sayısını loglar.

## Performans Karşılaştırması

`benchmark.sh` scripti, tek thread ile çoklu thread senaryolarını otomatik olarak kıyaslar. Her senaryo için taze test verisi üretilir; güvenilir ölçüm için `sudo` ile çalıştırılması önerilir (OS page cache'i temizler):

```bash
chmod +x benchmark.sh
sudo ./benchmark.sh
```

Gerçek ölçüm sonuçları (300 dosya, ~285 MB, SSD, temiz cache, iki çalıştırmanın ortalaması):

| Thread sayısı | Süre (s) | Hızlanma |
|--------------:|---------:|---------:|
| 1             |    1.442 |    1.00x |
| 2             |    0.962 |    1.50x |
| 4             |    0.774 |    1.86x |
| 8             |    0.805 |    1.79x |

**Gözlemler:**

- Thread sayısı arttıkça kopyalama süresi belirgin şekilde düşer; 4 thread en verimli noktadır (1.86x hızlanma).
- 8 thread'de hafif yavaşlama gözlemlenir: I/O-bound iş yükünde disk bant genişliği dolar, ek thread yalnızca mutex contention ve context-switch overhead getirir.
- Bu davranış I/O-bound iş yüklerinde beklenen bir örüntüdür.
- Sonuçlar diske (HDD/SSD), dosya boyut dağılımına ve CPU çekirdek sayısına göre değişir.

## Proje Yapısı

```text
.
├── Makefile
├── README.md
├── benchmark.sh
├── include/
│   ├── common.h
│   ├── queue.h
│   ├── scanner.h
│   ├── worker.h
│   └── log.h
├── src/
│   ├── main.c
│   ├── queue.c
│   ├── scanner.c
│   ├── worker.c
│   └── log.c
├── source_dir/         # Test kaynak dizini
└── dest_dir/           # Test hedef dizini
```

## Temizleme

```bash
make clean
```
