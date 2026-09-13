# Manual de Usuario — ExtreamFS

Guía paso a paso para instalar, levantar y usar ExtreamFS: la aplicación web que simula un sistema de archivos EXT2.

---

## Índice

1. [Requisitos previos](#1-requisitos-previos)
2. [Instalación](#2-instalación)
3. [Levantar la aplicación](#3-levantar-la-aplicación)
4. [Usar la interfaz](#4-usar-la-interfaz)
5. [Flujo completo de ejemplo](#5-flujo-completo-de-ejemplo)
6. [Solución de problemas comunes](#6-solución-de-problemas-comunes)

---

## 1. Requisitos previos

Necesitas tener instalado:

| Herramienta | Para qué se usa | Cómo verificar si ya la tienes |
|---|---|---|
| `g++` (con soporte C++17) | Compilar el backend | `g++ --version` |
| `make` | Compilar el backend con un solo comando | `make --version` |
| Node.js y `npm` | Correr el frontend en React | `node --version` y `npm --version` |
| Graphviz (`dot`) | Generar los reportes visuales | `dot -V` |

Si te falta Graphviz, instálalo con:

```bash
sudo apt update
sudo apt install graphviz
```

> Si `sudo apt update` te da un error sobre un repositorio `cdrom://`, ve a la sección [6. Solución de problemas comunes](#6-solución-de-problemas-comunes).

---

## 2. Instalación

No hay ningún paso de "instalación" tradicional — no se usan entornos virtuales, `.env`, ni gestores de paquetes de C++. Solo necesitas:

1. Clonar o descargar el repositorio del proyecto.
2. Confirmar que la carpeta `backend/` tenga la subcarpeta `include/` con los 7 archivos `.hpp` y el archivo `httplib.h`/`json.hpp`, y `src/main.cpp`.
3. Confirmar que la carpeta `frontend/` tenga `package.json`, `index.html`, y la carpeta `src/`.

---

## 3. Levantar la aplicación

La aplicación consta de **dos procesos independientes** que deben correr al mismo tiempo, cada uno en su propia terminal.

### Terminal 1 — Backend

```bash
cd backend
make
./server
```

Debe quedar mostrando:

```
ExtreamFS backend escuchando en http://localhost:8080
```

**No cierres esta terminal** mientras uses la aplicación.

### Terminal 2 — Frontend

Abre una terminal **nueva** (deja la del backend corriendo) y ejecuta:

```bash
cd frontend
npm install      # solo la primera vez, o si cambian las dependencias
npm run dev
```

Te va a mostrar una URL, normalmente:

```
➜  Local:   http://localhost:5173/
```

Ábrela en tu navegador.

---

## 4. Usar la interfaz

Al abrir la aplicación verás tres zonas principales:

- **Entrada** (arriba): donde escribes o pegas los comandos, uno por línea. También puedes cargar un archivo `.smia` con el botón "elegir archivo .smia" — su contenido se carga automáticamente en esta área.
- **Botones**: `ejecutar` (envía todo el contenido de la entrada al backend; también puedes usar `Ctrl+Enter` o `Cmd+Enter`) y `limpiar` (borra entrada y salida).
- **Salida** (abajo): muestra el resultado de cada comando en el mismo orden en que los escribiste. Las líneas que empiezan con `#` son comentarios y se muestran tal cual.

En la parte superior derecha hay un indicador de conexión con el backend: si dice **"sin conexión"** en rojo, verifica que el backend (`./server`) siga corriendo en su terminal.

La barra lateral derecha tiene una referencia rápida de los parámetros de los comandos más comunes.

---

## 5. Flujo completo de ejemplo

Copia y pega esto en el área de entrada, y presiona "ejecutar":

```
mkdisk -size=3000 -unit=K -path=/home/TU_USUARIO/Disco1.mia
fdisk -size=1000 -path=/home/TU_USUARIO/Disco1.mia -name=Part1 -unit=K
mount -path=/home/TU_USUARIO/Disco1.mia -name=Part1
mkfs -id=951A
login -user=root -pass=123 -id=951A
mkgrp -name=usuarios
mkusr -user=user1 -pass=usuario -grp=usuarios
mkfile -size=15 -path=/user/docs/a.txt -r
cat -file1=/user/docs/a.txt
```

> Reemplaza `TU_USUARIO` por tu nombre de usuario real, y `951A` por el id que te devuelva tu propio comando `mount` (depende de tu carnet).

Deberías ver en la salida algo como:

```
MKDISK: disco creado correctamente en /home/.../Disco1.mia (3072000 bytes)
FDISK: partición primaria "Part1" creada correctamente (1024000 bytes, inicia en byte 157)
MOUNT: partición "Part1" montada con id 951A
MKFS: partición formateada como EXT2 (3459 inodos, 10377 bloques)
LOGIN: sesión iniciada como "root" en la partición 951A
MKGRP: grupo "usuarios" creado con id 2
MKUSR: usuario "user1" creado con id 3
MKFILE: archivo "/user/docs/a.txt" creado (15 bytes)
012345678901234
```

### Generar un reporte

Con la sesión anterior todavía activa, agrega en la entrada:

```
rep -name=tree -path=/home/TU_USUARIO/reportes/tree.jpg -id=951A
```

Esto crea la carpeta `reportes/` si no existe, y genera `tree.jpg` con el árbol completo del sistema de archivos. Ábrelo con tu visor de imágenes habitual.

**Importante**: guarda tus reportes dentro de tu carpeta de usuario (`/home/tu_usuario/...`), no en `/tmp/`, ya que `/tmp` no persiste entre reinicios en una sesión USB live.

---

## 6. Solución de problemas comunes

### "no se pudo conectar con http://localhost:8080"
El backend no está corriendo. Ve a la Terminal 1 y confirma que `./server` sigue activo. Si se cerró, vuelve a ejecutarlo.

### El repositorio `cdrom://` falla al hacer `sudo apt update`
Es normal en una sesión Linux Mint live/USB: el sistema tiene registrado el propio medio de instalación como repositorio. Para arreglarlo:

```bash
sudo apt edit-sources
```

Busca la línea que empieza con `deb cdrom:` y coméntala (agrega `#` al inicio), guarda y cierra. Luego vuelve a correr `sudo apt update`.

Si el comando anterior no funciona, edita directamente:

```bash
sudo nano /etc/apt/sources.list.d/official-package-repositories.list
```

### Los reportes (`REP`) fallan con "¿está instalado graphviz?"
Falta el paquete `graphviz`. Instálalo con `sudo apt install graphviz` y confirma con `dot -V`.

### Generé un reporte pero no lo encuentro
Revisa la ruta exacta que usaste en `-path`. Si usaste algo dentro de `/tmp/`, ese archivo puede no ser visible desde tu gestor de archivos gráfico por defecto — navega directo a esa ruta, o mejor, usa una ruta dentro de tu carpeta personal (`/home/tu_usuario/...`) para la próxima vez.

### "MKGRP: no hay una sesión activa" (o similar en otros comandos)
Todos los comandos de usuarios/grupos/archivos (salvo `MKFS` y `LOGIN`) requieren haber iniciado sesión antes con `LOGIN`. Verifica que tu script incluya el `login` antes de esos comandos, y que no hayas cerrado sesión sin querer.

### "ya hay una sesión activa" al intentar hacer LOGIN
Solo puede haber una sesión iniciada a la vez en todo el sistema. Ejecuta `logout` primero.

### Un comando de FDISK/MOUNT/MKFS dice que el id o la partición "no existe"
El id de montaje (por ejemplo `951A`) solo vive en la memoria del proceso del backend — si reiniciaste `./server`, todas las particiones montadas anteriormente se "desmontan" (aunque el disco y sus particiones siguen intactos en el archivo `.mia`). Vuelve a montar la partición con `mount` para obtener un id válido.