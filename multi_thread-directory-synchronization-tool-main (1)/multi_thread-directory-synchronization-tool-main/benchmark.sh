#!/usr/bin/env bash
# Tek thread vs cok thread performans karsilastirmasi
#
# Kullanim:  chmod +x benchmark.sh && ./benchmark.sh
#         veya: sudo ./benchmark.sh   (page cache temizleme icin)
#
# Not: copy_tool argumanlarinin formati:
#   ./copy_tool <thread_sayisi> <kaynak> <hedef>

set -e
export LC_ALL=C  # ondalik ayirici olarak '.' kullan (awk locale hatasi onlenir)

TOOL="./copy_tool"
SRC="./bench_src"
DST_PREFIX="./bench_dst"
THREADS=(1 2 4 8)
NUM_FILES=300
MIN_KB=10
MAX_KB=2000

if [ ! -x "$TOOL" ]; then
    echo "Hata: $TOOL bulunamadi. Once 'make' calistirin." >&2
    exit 1
fi

# Her olcum icin taze kaynak verisi olusturan yardimci fonksiyon
generate_src() {
    rm -rf "$SRC"
    mkdir -p "$SRC/alt_dizin_1" "$SRC/alt_dizin_2/derin"
    for i in $(seq 1 "$NUM_FILES"); do
        size=$((RANDOM % (MAX_KB - MIN_KB) + MIN_KB))
        case $((i % 3)) in
            0) dir="$SRC" ;;
            1) dir="$SRC/alt_dizin_1" ;;
            2) dir="$SRC/alt_dizin_2/derin" ;;
        esac
        dd if=/dev/urandom of="$dir/dosya_$i.bin" bs=1024 count="$size" 2>/dev/null
    done
}

echo "==> Ilk test verisi olusturuluyor ($NUM_FILES dosya, $MIN_KB-${MAX_KB}KB araliginda)..."
generate_src
TOTAL_SIZE=$(du -sh "$SRC" | cut -f1)
echo "==> Ortalama test verisi boyutu: $TOTAL_SIZE"
echo ""
echo "+----------------+-----------+------------+"
echo "| Thread sayisi  | Sure (s)  | Hizlanma   |"
echo "+----------------+-----------+------------+"

BASE_TIME=""
for N in "${THREADS[@]}"; do
    # Her olcum icin taze kaynak verisi olustur (page cache etkisini azaltir)
    generate_src

    DST="${DST_PREFIX}_${N}"
    rm -rf "$DST"
    mkdir -p "$DST"

    # Page cache'i temizle (root yetkisi varsa)
    sync 2>/dev/null || true
    if [ -w /proc/sys/vm/drop_caches ]; then
        echo 3 > /proc/sys/vm/drop_caches
    fi

    START=$(date +%s.%N)
    "$TOOL" "$N" "$SRC" "$DST" > /dev/null 2>&1
    END=$(date +%s.%N)

    DUR=$(awk "BEGIN {print $END - $START}")
    if [ -z "$BASE_TIME" ]; then
        BASE_TIME="$DUR"
        SPEEDUP="1.00x"
    else
        SPEEDUP=$(awk "BEGIN {printf \"%.2fx\", $BASE_TIME / $DUR}")
    fi
    printf "| %14d | %9.3f | %10s |\n" "$N" "$DUR" "$SPEEDUP"
done
echo "+----------------+-----------+------------+"

echo ""
echo "==> Temizlik yapiliyor..."
rm -rf "$SRC" "${DST_PREFIX}_"*
echo "==> Bitti."
