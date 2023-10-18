objcopy --only-keep-debug build/package/addons/cs2fixes/bin/linuxsteamrt64/cs2fixes.so build/cs2fixes.debug
sentry-cli debug-files upload -o csco-gk -p cs2fixes build/cs2fixes.debug
sentry-cli debug-files bundle-sources ./build/cs2fixes.debug
sentry-cli debug-files upload -o csco-gk -p cs2fixes --type sourcebundle ./build/cs2fixes.src.zip