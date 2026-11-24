const CACHE_NAME = 'walle-cockpit-v1';
const STATIC_ASSETS = [
  '/',
  '/manifest.json',
  '/static/icons/icon-192.png',
  '/static/icons/icon-512.png',
  '/static/icons/icon-180.png',
  '/static/icons/favicon.png'
];

// Instalação: Pré-cache de casca e ícones
self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      return cache.addAll(STATIC_ASSETS);
    })
  );
  self.skipWaiting();
});

// Ativação: Limpeza de caches antigos
self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) => {
      return Promise.all(
        keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))
      );
    })
  );
  self.clients.claim();
});

// Interceptação de requisições:
// Regra de Ouro: Comandos do robô, telemetria e streaming de vídeo NUNCA são cacheados!
self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);

  // Streaming, APIs, Telemetria e Comandos de Controle -> Sempre direto na Rede
  if (
    url.pathname.startsWith('/video_feed') ||
    url.pathname.startsWith('/telemetry') ||
    url.pathname.startsWith('/drive') ||
    url.pathname.startsWith('/servo') ||
    url.pathname.startsWith('/stop') ||
    url.pathname.startsWith('/expr') ||
    url.pathname.startsWith('/api/') ||
    url.pathname.startsWith('/sounds_list') ||
    url.pathname.startsWith('/play_sound') ||
    url.pathname.startsWith('/photos') ||
    url.pathname.startsWith('/action') ||
    url.pathname.startsWith('/toggle') ||
    url.pathname.startsWith('/volume') ||
    event.request.method !== 'GET'
  ) {
    return; // Passa direto para a rede sem mexer no cache
  }

  // Assets estáticos e HTML da casca -> Network First com fallback de cache
  event.respondWith(
    fetch(event.request)
      .then((networkResponse) => {
        if (networkResponse && networkResponse.status === 200) {
          const resClone = networkResponse.clone();
          caches.open(CACHE_NAME).then((cache) => {
            cache.put(event.request, resClone);
          });
        }
        return networkResponse;
      })
      .catch(() => {
        return caches.match(event.request).then((cached) => {
          if (cached) return cached;
          if (event.request.mode === 'navigate') {
            return caches.match('/');
          }
        });
      })
  );
});
