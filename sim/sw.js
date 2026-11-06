// XO-Hugbox emulator service worker: the installed app works offline.
// The page (index.html, which carries the whole emulator) is network-first, so a new version shows up on the next
// launch with a connection; offline it comes from the cache. Icons and the manifest are cache-first; the web fonts
// are served from the cache and refreshed in the background. All paths are relative, so it works under any folder.
const CACHE = 'hugbox-v1';
const CORE = ['./', './index.html', './manifest.webmanifest', './icons/icon-192.png', './icons/icon-512.png',
  './icons/icon-maskable-512.png', './icons/apple-touch-icon.png'];
const FONTS = /^https:\/\/fonts\.(googleapis|gstatic)\.com\//;

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(CORE)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys()
    .then((keys) => Promise.all(keys.filter((k) => k.startsWith('hugbox-') && k !== CACHE).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});

self.addEventListener('fetch', (e) => {
  const req = e.request;
  if (req.method !== 'GET') return;
  const url = new URL(req.url);
  const scope = new URL('./', self.registration.scope);
  const page = req.mode === 'navigate' ||
    (url.origin === scope.origin && (url.pathname === scope.pathname || url.pathname === scope.pathname + 'index.html'));
  if (page) {                                         // network first, then the cached page
    e.respondWith(fetch(req).then((res) => {
      if (res.ok) { const copy = res.clone(); caches.open(CACHE).then((c) => c.put('./index.html', copy)); }
      return res;
    }).catch(() => caches.match('./index.html').then((r) => r || caches.match('./'))));
    return;
  }
  if (url.origin === scope.origin) {                  // our files: cache first
    e.respondWith(caches.match(req).then((hit) => hit || fetch(req).then((res) => {
      if (res.ok) { const copy = res.clone(); caches.open(CACHE).then((c) => c.put(req, copy)); }
      return res;
    })));
    return;
  }
  if (FONTS.test(req.url)) {                          // fonts: cached copy now, fresh copy for next time
    e.respondWith(caches.open(CACHE).then((c) => c.match(req).then((hit) => {
      const net = fetch(req).then((res) => { if (res.ok || res.type === 'opaque') c.put(req, res.clone()); return res; })
        .catch(() => hit || Response.error());
      return hit || net;
    })));
  }
});
