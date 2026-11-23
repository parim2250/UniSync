#!/usr/bin/env bash
# End-to-end tests for UniSync (two instances on localhost). Run with:  make test
cd "$(dirname "$0")/.."
BIN=$PWD/bin/UniSync
T=$(mktemp -d); RPID=""
cleanup() { [ -n "$RPID" ] && kill $RPID 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT
pass=0; fail=0
ok()  { echo "  PASS: $1"; pass=$((pass+1)); }
bad() { echo "  FAIL: $1"; fail=$((fail+1)); }
RH=$T/rhome; SH=$T/shome; mkdir -p $RH $SH
rx()   { HOME=$RH NO_COLOR=1 exec $BIN receive --port 47101 "$@"; }        # receiver
tx()   { HOME=$SH NO_COLOR=1 $BIN send "$@"; }                         # sender
stop() { [ -n "$RPID" ] && kill $RPID 2>/dev/null; wait $RPID 2>/dev/null; RPID=""; sleep 0.5; }

echo "== 1. Test vectors (SHA-256, ChaCha20)"
$BIN selftest | grep -q FAIL && bad "selftest" || ok "SHA-256 + ChaCha20 official test vectors"
head -c 1000003 /dev/urandom > $T/r.bin
[ "$($BIN hash $T/r.bin | cut -d' ' -f1)" = "$(sha256sum $T/r.bin | cut -d' ' -f1)" ] && ok "SHA-256 matches sha256sum" || bad "SHA-256 mismatch"

echo "== 2. Prepare data"
mkdir -p $T/src/proj/sub/deep $T/src/proj/emptydir
head -c 5000000 /dev/urandom > $T/src/big.bin
echo "hello unisync" > "$T/src/file with spaces.txt"
echo "main" > $T/src/proj/main.c; head -c 300000 /dev/urandom > $T/src/proj/sub/data.bin
echo "deep" > $T/src/proj/sub/deep/x.txt; : > $T/src/proj/empty.txt
chmod 755 $T/src/proj/main.c

echo "== 3. File + folder transfer with PIN"
rx --dir $T/dest --pin 4321 --auto-accept >$T/recv.log 2>&1 & RPID=$!
sleep 1
tx --to 127.0.0.1:47101 --pin 4321 $T/src/big.bin "$T/src/file with spaces.txt" $T/src/proj >$T/send.log 2>&1
[ $? -eq 0 ] && ok "send exit code 0" || { bad "send failed"; cat $T/send.log; }
diff -r $T/src/proj $T/dest/proj >/dev/null && ok "folder identical (nested, empty dir, empty file)" || bad "folder differs"
cmp $T/src/big.bin $T/dest/big.bin && ok "big.bin identical" || bad "big.bin differs"
cmp "$T/src/file with spaces.txt" "$T/dest/file with spaces.txt" && ok "name with spaces ok" || bad "spaces"
[ "$(stat -c %a $T/dest/proj/main.c)" = "755" ] && ok "permissions preserved (chmod 755)" || bad "permissions"

echo "== 4. Wrong PIN refused"
tx --to 127.0.0.1:47101 --pin 0000 $T/src/proj/main.c >$T/bad.log 2>&1
[ $? -ne 0 ] && grep -q "wrong PIN" $T/bad.log && ok "wrong PIN denied" || bad "wrong PIN not denied"

echo "== 5. Interrupted transfer resumes"
rm -f $T/dest/big.bin
tx --to 127.0.0.1:47101 --pin 4321 --simulate-drop 2000000 $T/src/big.bin >$T/drop.log 2>&1
grep -q "connection lost" $T/drop.log && ok "connection was cut" || bad "no drop"
grep -q "Resuming big.bin from" $T/recv.log && ok "receiver resumed from partial data" || bad "no resume"
cmp $T/src/big.bin $T/dest/big.bin && ok "file complete + identical after resume" || bad "file differs after resume"
ls $T/dest | grep -q unisync && bad "leftover temp files" || ok "no leftover .part/.meta files"

echo "== 6. Corrupted partial data caught by SHA-256"
rm -f $T/dest/big.bin
head -c 1000000 $T/src/big.bin > $T/dest/big.bin.unisync.part
printf '%s %s\n' 5000000 "$($BIN hash $T/src/big.bin | cut -d' ' -f1)" > $T/dest/big.bin.unisync.meta
printf 'X' | dd of=$T/dest/big.bin.unisync.part bs=1 seek=500 conv=notrunc 2>/dev/null
tx --to 127.0.0.1:47101 --pin 4321 $T/src/big.bin >$T/corrupt.log 2>&1
grep -q "corrupted in transit" $T/corrupt.log && ok "corruption detected" || bad "corruption missed"
cmp $T/src/big.bin $T/dest/big.bin && ok "automatic retry delivered a correct file" || bad "no correct file"

echo "== 7. Four simultaneous transfers"
mkdir -p $T/c; for i in 1 2 3 4; do head -c 3000000 /dev/urandom > $T/c/f$i.bin; done
for i in 1 2 3 4; do HOME=$T/sh$i NO_COLOR=1 $BIN send --name S$i --to 127.0.0.1:47101 --pin 4321 $T/c/f$i.bin >$T/c$i.log 2>&1 & done
sleep 6
okc=1; for i in 1 2 3 4; do cmp -s $T/c/f$i.bin $T/dest/f$i.bin || okc=0; done
[ $okc = 1 ] && ok "4 concurrent transfers all correct" || bad "concurrent"

echo "== 8. History and log"
HOME=$RH NO_COLOR=1 $BIN history 50 > $T/hist.txt
grep -q SUCCESS $T/hist.txt && ok "history shows SUCCESS" || bad "history"
grep -q AUTH-FAILED $T/hist.txt && ok "history records bad PIN attempt" || bad "history auth"
[ -s $RH/.unisync/unisync.log ] && ok "log file written" || bad "log"
stop

echo "== 9. Device discovery"
rx --dir $T/dest9 --auto-accept --name DiscoverMe >$T/r9.log 2>&1 & RPID=$!
sleep 1
echo "disc" > $T/disc.txt
HOME=$SH NO_COLOR=1 $BIN devices > $T/dev.txt 2>&1
grep -q DiscoverMe $T/dev.txt && ok "'devices' finds the receiver" || { bad "devices"; cat $T/dev.txt; }
tx $T/disc.txt >$T/d9.log 2>&1 </dev/null
cmp $T/disc.txt $T/dest9/disc.txt && ok "send without --to: auto-discovered the receiver" || { bad "auto-discovery send"; cat $T/d9.log; }
stop

echo "== 10. Encrypted transport"
rx --dir $T/edest --pin 9999 --encrypt --auto-accept >$T/e.log 2>&1 & RPID=$!
sleep 1
cat > $T/proxy.py <<'PY'
import socket, threading, sys
cap = open(sys.argv[1], "wb")
def pipe(a, b):
    while True:
        try: d = a.recv(65536)
        except Exception: break
        if not d: break
        cap.write(d); cap.flush()
        try: b.sendall(d)
        except Exception: break
    try: b.shutdown(socket.SHUT_WR)
    except Exception: pass
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 47301)); srv.listen(1); srv.settimeout(20)
c, _ = srv.accept(); u = socket.create_connection(("127.0.0.1", 47101))
t = threading.Thread(target=pipe, args=(c, u)); t.start(); pipe(u, c); t.join(); cap.close()
PY
timeout 30 python3 $T/proxy.py $T/capture.bin </dev/null >/dev/null 2>&1 &
sleep 1
python3 -c "open('$T/secret.txt','w').write('TOP-SECRET-MARKER-12345 '*2000)"
tx --to 127.0.0.1:47301 --pin 9999 $T/secret.txt >$T/enc.log 2>&1
sleep 2
cmp $T/secret.txt $T/edest/secret.txt && ok "encrypted transfer delivers identical file" || { bad "encrypted transfer"; cat $T/enc.log; }
grep -q "TOP-SECRET-MARKER" $T/capture.bin && bad "plaintext visible on the wire!" || ok "wire capture shows no plaintext"
tx --to 127.0.0.1:47101 --pin 9999 $T/src/proj/main.c >$T/e2.log 2>&1
cmp $T/src/proj/main.c $T/edest/main.c && ok "receiver that requires encryption makes the sender encrypt automatically" || bad "auto-encrypt"
stop

echo "== 11. Accept / reject prompt (y / n) and pairing (a)"
( sleep 3; echo n; sleep 6; echo y; sleep 6; echo a ) | rx --dir $T/pdest >$T/p.log 2>&1 & RPID=$!
sleep 1
echo one > $T/one.txt
tx --to 127.0.0.1:47101 $T/one.txt >$T/p1.log 2>&1
grep -q "rejected" $T/p1.log && [ ! -f $T/pdest/one.txt ] && ok "answer 'n' -> sender told it was rejected, nothing saved" || bad "reject flow"
tx --to 127.0.0.1:47101 $T/one.txt >$T/p2.log 2>&1
cmp $T/one.txt $T/pdest/one.txt && ok "answer 'y' -> file received" || { bad "accept flow"; cat $T/p2.log; }
echo two > $T/two.txt
tx --name Foo --to 127.0.0.1:47101 $T/two.txt >$T/p3.log 2>&1
cmp $T/two.txt $T/pdest/two.txt && ok "answer 'a' -> received and device paired" || bad "always flow"
grep -qx Foo $RH/.unisync/paired.txt && ok "paired.txt saved" || bad "paired.txt"
echo three > $T/three.txt
tx --name Foo --to 127.0.0.1:47101 $T/three.txt >$T/p4.log 2>&1 </dev/null
cmp $T/three.txt $T/pdest/three.txt && ok "paired device accepted with no question" || bad "paired not auto-accepted"
stop

echo "== 12. Post-receive hook (fork/exec) and file type logging"
printf '#!/bin/sh\necho "$1" >> %s/hook.out\n' "$T" > $T/hook.sh; chmod +x $T/hook.sh
rx --dir $T/hdest --auto-accept --exec $T/hook.sh >$T/h.log 2>&1 & RPID=$!
sleep 1
tx --to 127.0.0.1:47101 $T/src/proj/main.c >$T/h1.log 2>&1
sleep 1
grep -q "main.c" $T/hook.out 2>/dev/null && ok "--exec program ran via fork+exec" || bad "hook"
grep -q "type c" $RH/.unisync/unisync.log && ok "file type logged" || bad "file type"
stop

echo; echo "Result: $pass passed, $fail failed"
[ $fail -eq 0 ]
