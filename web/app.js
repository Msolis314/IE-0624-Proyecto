'use strict';

/*
* Aplicacion de control de Junior
 * Mientras un botón de dirección está presionado se repite "MOV xxx" cada
 * 100 ms. La ESP32 detiene la plataforma si pasan 500 ms sin repetición,
 * así que soltar el botón, perder el WiFi o cerrar la pestaña la detiene.
 * Al soltar se envía además "ALTO" para no esperar el timeout.
*/


const PERIODO_REPETICION_MS = 100;
const ESPERA_RECONEXION_MS = 1000;
const TELEMETRIA_VIEJA_MS = 1500;

const NOMBRE_MOV = {ADE: 'Adelante', ATR: 'Atrás', IZQ: 'Izquierda', DER: 'Derecha', NIN: 'Detenido'};

const $ = (id) => document.getElementById(id);

let ws = null;
let movActual = null;
let temporizador = null;
let ultimaTelemetria = null;

/* Conexion WebSocket */

function conectar () {
  ws = new WebSocket(`ws://${location.host}/ws`);

  ws.onopen = () => mostrarConexion('ok');
  ws.onclose = () => {
    terminarMovimiento(false);
    mostrarConexion('mal');
    setTimeout(conectar, ESPERA_RECONEXION_MS);
  };
  ws.onerror = () => ws.close();
  ws.onmessage = (ev) => {
    try {
      mostrarEstado(JSON.parse(ev.data));
      ultimaTelemetria = Date.now();
    } catch (e) {
      console.warn('telemetría inválida', ev.data);
    }
  };
}

function enviar(texto) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(texto);
    }
}

function mostrarConexion(estado) {
  const chip = $('conexion');
  const textos = { ok: 'Conectado', mal: 'Desconectado', aviso: 'Sin datos' };
  chip.textContent = textos[estado];
  chip.className = 'chip ' + (estado === 'ok' ? 'chip-ok' : estado === 'aviso' ? 'chip-aviso' : 'chip-mal');
  document.body.classList.toggle('sin-conexion', estado !== 'ok');
}

/* Si llega conexión pero no telemetría, algo anda mal en la ESP32 */
setInterval(() => {
  if (ws && ws.readyState === WebSocket.OPEN) {
    mostrarConexion(Date.now() - ultimaTelemetria > TELEMETRIA_VIEJA_MS ? 'aviso' : 'ok');
  }
}, 500);


// Movimiento

function iniciarMovimiento(mov) {
  if (movActual === mov) return;
  clearInterval(temporizador);
  movActual = mov;
  enviar('MOV ' + mov);
  temporizador = setInterval(() => enviar('MOV ' + movActual), PERIODO_REPETICION_MS);
  resaltar(mov);
}

function terminarMovimiento(enviarAlto = true) {
  if (movActual === null) return;
  clearInterval(temporizador);
  temporizador = null;
  movActual = null;
  if (enviarAlto) enviar('ALTO');
  resaltar(null);
}

function resaltar(mov) {
  document.querySelectorAll('.dir').forEach((b) => {
    b.classList.toggle('activo', b.dataset.mov === mov);
  });
}

document.querySelectorAll('.dir').forEach((boton) => {
  const mov = boton.dataset.mov;
  boton.addEventListener('pointerdown', (ev) => {
    ev.preventDefault();
    iniciarMovimiento(mov);
  });
  /* Soltar, salir del botón o que el sistema cancele el toque detienen */
  ['pointerup', 'pointerleave', 'pointercancel'].forEach((tipo) =>
    boton.addEventListener(tipo, () => {
      if (movActual === mov) terminarMovimiento();
    })
  );
  boton.addEventListener('contextmenu', (ev) => ev.preventDefault());
});

/* Cambiar de pestaña o de aplicación también detiene */
window.addEventListener('blur', () => terminarMovimiento());
document.addEventListener('visibilitychange', () => {
  if (document.hidden) terminarMovimiento();
});

// Otros botones

$('alto').addEventListener('click', () => { terminarMovimiento(false); enviar('ALTO'); });
$('paro').addEventListener('pointerdown', () => { terminarMovimiento(false); enviar('PARO'); });
$('reanudar').addEventListener('click', () => enviar('REANUDAR'));
$('vel-mas').addEventListener('click', () => enviar('VEL +'));
$('vel-menos').addEventListener('click', () => enviar('VEL -'));
$('modo-manual').addEventListener('click', () => enviar('MODO MANUAL'));
$('modo-luz').addEventListener('click', () => enviar('MODO LUZ'));


// Teclado (laptop)

const TECLAS_MOV = {
  w: 'ADE', ArrowUp: 'ADE',
  s: 'ATR', ArrowDown: 'ATR',
  a: 'IZQ', ArrowLeft: 'IZQ',
  d: 'DER', ArrowRight: 'DER',
};

document.addEventListener('keydown', (ev) => {
  const mov = TECLAS_MOV[ev.key];
  if (mov) {
    ev.preventDefault();
    if (!ev.repeat) iniciarMovimiento(mov);
    return;
  }
  if (ev.repeat) return;
  switch (ev.key) {
    case 'p': case 'P': case ' ': terminarMovimiento(false); enviar('PARO'); break;
    case 'x': case 'X': terminarMovimiento(false); enviar('ALTO'); break;
    case 'r': case 'R': enviar('REANUDAR'); break;
    case '+': enviar('VEL +'); break;
    case '-': enviar('VEL -'); break;
  }
});

document.addEventListener('keyup', (ev) => {
  const mov = TECLAS_MOV[ev.key];
  if (mov && movActual === mov) terminarMovimiento();
});


// Telemetria

function mostrarBarra(idBarra, idTexto, valor) {
  const barra = $(idBarra);
  const v = Math.max(-100, Math.min(100, valor));
  const ancho = Math.abs(v) / 2;
  barra.style.width = ancho + '%';
  barra.style.left = v >= 0 ? '50%' : (50 - ancho) + '%';
  barra.classList.toggle('negativo', v < 0);
  $(idTexto).textContent = (v > 0 ? '+' : '') + v.toFixed(0) + ' %';
}

function mostrarEstado(e) {
  const hab = $('habilitado');
  hab.textContent = e.hab ? 'Habilitados' : 'En paro';
  hab.className = 'chip ' + (e.hab ? 'chip-ok' : 'chip-mal');

  $('modo').textContent = e.modo === 'luz' ? 'Luz' : 'Manual';
  $('movimiento').textContent = (NOMBRE_MOV[e.mov] || e.mov) + (e.mov !== 'NIN' ? ` (${e.origen})` : '');
  $('vb').textContent = e.vb + ' %';
  $('lim').textContent = `límite ${e.lim} %`;

  mostrarBarra('barra-izq', 'salida-izq', e.izq);
  mostrarBarra('barra-der', 'salida-der', e.der);

  $('modo-manual').classList.toggle('activo', e.modo === 'manual');
  $('modo-luz').classList.toggle('activo', e.modo === 'luz');

  $('clientes').textContent = e.clientes;
  $('timeouts').textContent = e.timeouts;
  $('perdidos').textContent = e.perdidos;
  $('ciclo').textContent = e.ciclo;
}

mostrarConexion('mal');
conectar();