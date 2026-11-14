// Chapters, transcript and end-card for the narrated MP4. Needs only narration.js, so the page also works without the film renderer.
(() => {
  const video = document.getElementById('tour-video'), chapters = document.getElementById('chapters'), next = document.getElementById('film-next');
  const LABEL = { intro: 'Intro', overview: 'Three parts', chain: 'How it flows', sensor: 'The sensor', rs485: 'RS485', power: 'Power', setup: 'Setup', viking: 'Viking Analytics', do: 'DO0 relay', di: 'DI1 acknowledge', alarm: 'See it work', rule: 'The alarm rule', outro: 'Sense. Analyse. Act.' };
  const S = NARRATION.scenes, fmt = s => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, '0')}`;
  const playAt = t => { video.currentTime = t; video.play().catch(() => {}); };
  const at = t => { let i = S.length - 1; while (i > 0 && S[i].start > t) i--; return i; };
  chapters.innerHTML = S.map((s, i) => `<button data-i="${i}" aria-label="${LABEL[s.id] || s.id}, ${fmt(s.start)}">${LABEL[s.id] || s.id}</button>`).join('');
  chapters.addEventListener('click', e => { const b = e.target.closest('[data-i]'); if (b) playAt(S[+b.dataset.i].start); });
  video.addEventListener('timeupdate', () => { const c = at(video.currentTime); chapters.querySelectorAll('button').forEach((b, i) => { b.classList.toggle('on', i === c); i === c ? b.setAttribute('aria-current', 'true') : b.removeAttribute('aria-current'); }); });
  video.addEventListener('ended', () => next.classList.add('finished'));
  video.addEventListener('seeking', () => { if (video.currentTime < video.duration - .25) next.classList.remove('finished'); });
  video.addEventListener('error', () => { document.getElementById('film-status').textContent = 'The film could not load. Try the MP4 download below.'; });
  const tr = document.getElementById('tour-transcript');
  S.forEach(s => { const p = document.createElement('p'), b = document.createElement('button'); b.type = 'button'; b.textContent = fmt(s.start); b.setAttribute('aria-label', 'Jump to ' + (LABEL[s.id] || s.id)); b.onclick = () => playAt(s.start); p.append(b, document.createTextNode(s.text)); tr.append(p); });
  new IntersectionObserver(es => { if (!es[0].isIntersecting && !video.paused) video.pause(); }, { threshold: .2 }).observe(video);
})();
