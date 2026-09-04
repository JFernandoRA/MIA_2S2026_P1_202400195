# Manual Técnico — ExtreamFS (Proyecto 1, C++ Disk)

**Universidad de San Carlos de Guatemala — Facultad de Ingeniería**
**Manejo e Implementación de Archivos — 2do. Semestre 2026**

José Fernando Ramírez Ambrocio — Carnet 202400195

---

## Índice

1. [Arquitectura del Sistema](#1-arquitectura-del-sistema)
2. [Estructuras de Datos](#2-estructuras-de-datos)
3. [Comandos Implementados](#3-comandos-implementados)
4. [Limitaciones Conocidas](#4-limitaciones-conocidas)
5. [Instrucciones de Ejecución](#5-instrucciones-de-ejecución)

---

## 1. Arquitectura del Sistema

ExtreamFS es una aplicación web que simula el funcionamiento interno de un sistema de archivos EXT2 sobre archivos binarios (`.mia`), sin depender de particiones físicas reales. La arquitectura sigue el modelo cliente-servidor definido en el enunciado del proyecto, separando completamente la interfaz de usuario del motor que administra el sistema de archivos.

### 1.1 Diagrama general

```
┌─────────────────────────── localhost ───────────────────────────┐
│                                                                    │
│  ┌─────────────────────┐   HTTP (JSON)   ┌──────────────────────┐│
│  │      FRONTEND         │ ─────────────▶ │       BACKEND         ││
│  │  React + Vite           │              │  C++17 (cpp-httplib)   ││
│  │  localhost:5173          │ ◀───────────  │  localhost:8080        ││
│  └─────────────────────┘   POST /execute  └───────────┬──────────┘│
│                                                          │          │
│                                                          ▼          │
│                                            ┌────────────────────┐  │
│                                            │  Archivos .mia       │  │
│                                            │  (sistema de archivos │  │
│                                            │   del SO anfitrión)   │  │
│                                            └────────────────────┘  │
└────────────────────────────────────────────────────────────────────┘
```

### 1.2 Frontend

Construido con React 19 y Vite. Consiste en una página única con:

- Área de entrada de comandos (editable), con carga de archivos `.smia` mediante un selector de archivos.
- Botón **ejecutar** que envía todo el contenido del área de entrada al backend en una sola petición.
- Botón **limpiar** que reinicia ambas áreas.
- Área de salida (solo lectura) que muestra la respuesta del backend línea por línea, en el mismo orden de ejecución.
- Indicador de estado de conexión con el backend.

El frontend no contiene ninguna lógica de negocio: únicamente envía el texto crudo de los comandos al backend vía `POST /execute` y muestra la respuesta tal cual la recibe. Toda la validación, parsing y manipulación de las estructuras del sistema de archivos ocurre exclusivamente en el backend, en cumplimiento del enunciado.

### 1.3 Backend

Implementado en C++17. Expone una API RESTful mínima mediante la librería header-only `cpp-httplib`:

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/` | Verificación de que el servidor está activo (health check) |
| POST | `/execute` | Recibe `{ "commands": "<texto>" }` con uno o más comandos separados por saltos de línea, los ejecuta en orden y devuelve `{ "output": "<texto>" }` con el resultado de cada uno |

El backend está organizado en módulos de responsabilidad única:

| Archivo | Responsabilidad |
|---|---|
| `structs.hpp` | Estructuras binarias MBR, Partition, EBR |
| `ext2_structs.hpp` | Estructuras binarias Superbloque, Inodo, Bloques |
| `parser.hpp` | Tokenización y parseo de comandos (`-clave=valor`) |
| `disk_utils.hpp` | Lectura/escritura del MBR/EBR, cálculo de espacio libre (fit) |
| `ext2_utils.hpp` | Lectura/escritura genérica de structs y bytes sueltos en el disco |
| `mount_manager.hpp` | Estado de particiones montadas en RAM, generación de IDs |
| `session.hpp` | Sesión de usuario activa en RAM |
| `fs_ops.hpp` | Operaciones de bajo nivel EXT2: bitmaps, inodos, bloques, rutas |
| `users_file.hpp` | Parser lógico del archivo `users.txt` |
| `commands.hpp` | MKDISK, RMDISK, FDISK, MOUNT, MOUNTED |
| `ext2_commands.hpp` | MKFS |
| `account_commands.hpp` | LOGIN, LOGOUT, MKGRP, RMGRP, MKUSR, RMUSR, CHGRP, CAT |
| `path_commands.hpp` | MKFILE, MKDIR |
| `rep_commands.hpp` / `rep_utils.hpp` | REP (los 10 tipos de reporte) y utilidades de Graphviz |
| `main.cpp` | Servidor HTTP y despachador de comandos |

### 1.4 Persistencia y estado en memoria

Siguiendo el enunciado, existen dos niveles de persistencia claramente diferenciados:

- **En disco (`.mia`)**: el MBR, las particiones, y — una vez formateada una partición con MKFS — el superbloque, bitmaps, inodos y bloques del sistema de archivos EXT2, incluyendo el archivo `users.txt`.
- **En memoria RAM del proceso backend**: qué particiones están montadas y con qué id, y la sesión de usuario actualmente autenticada. Esta información se pierde al reiniciar el servidor, tal como especifica el enunciado para el comando MOUNT.

---

## 2. Estructuras de Datos

Todas las estructuras se declaran con `#pragma pack(1)` para eliminar el padding del compilador, garantizando que el layout de bytes en el archivo `.mia` sea exacto y predecible, tal como exige el enunciado.

### 2.1 MBR (Master Boot Record)

Se escribe en el primer sector de cada disco. Contiene la información general del disco y sus 4 particiones.

| Campo | Tipo | Descripción |
|---|---|---|
| `mbr_tamano` | int | Tamaño total del disco en bytes |
| `mbr_fecha_creacion` | time_t | Fecha y hora de creación |
| `mbr_dsk_signature` | int | Número aleatorio único del disco |
| `dsk_fit` | char | Ajuste por defecto del disco (B/F/W) |
| `mbr_partitions` | Partition[4] | Las 4 particiones del disco |

### 2.2 Partition

| Campo | Tipo | Descripción |
|---|---|---|
| `part_status` | char | `'0'` libre / `'1'` montada |
| `part_type` | char | `'P'` primaria, `'E'` extendida |
| `part_fit` | char | Ajuste B/F/W |
| `part_start` | int | Byte del disco donde inicia |
| `part_s` | int | Tamaño total en bytes |
| `part_name` | char[16] | Nombre de la partición |
| `part_correlative` | int | -1 hasta que se monta; luego el número de montaje |
| `part_id` | char[4] | ID generado al montar |

### 2.3 EBR (Extended Boot Record)

Descriptor de una partición lógica dentro de una extendida. Funciona como una lista enlazada: cada EBR apunta al byte donde inicia el siguiente.

| Campo | Tipo | Descripción |
|---|---|---|
| `part_mount` | char | Montada o no |
| `part_fit` | char | Ajuste B/F/W |
| `part_start` | int | Byte donde inicia |
| `part_s` | int | Tamaño en bytes |
| `part_next` | int | Byte del siguiente EBR, -1 si no hay |
| `part_name` | char[16] | Nombre de la partición lógica |

### 2.4 Superbloque

Se escribe una sola vez al inicio de la partición formateada, en la posición `part_start`. Contiene los metadatos globales del sistema de archivos EXT2 de esa partición.

| Campo | Descripción |
|---|---|
| `s_filesystem_type` | Identificador del sistema de archivos (2 = EXT2) |
| `s_inodes_count` / `s_blocks_count` | Total de inodos / bloques calculados |
| `s_free_blocks_count` / `s_free_inodes_count` | Libres actualmente |
| `s_mtime` / `s_umtime` | Último montaje / desmontaje |
| `s_mnt_count` | Veces que se ha montado |
| `s_magic` | Firma `0xEF53` |
| `s_inode_s` / `s_block_s` | Tamaño de un inodo / de un bloque (64 bytes) |
| `s_firts_ino` / `s_first_blo` | Primer inodo / bloque libre |
| `s_bm_inode_start` / `s_bm_block_start` | Inicio de cada bitmap |
| `s_inode_start` / `s_block_start` | Inicio de la tabla de inodos / bloques |

### 2.5 Cálculo de `n` (inodos y bloques)

El número de bloques siempre es el triple del número de inodos. `n` se despeja de:

```
tamaño_particion = sizeOf(superblock) + n + 3n + n·sizeOf(inodo) + 3n·sizeOf(block)

n = floor((tamaño_particion − sizeOf(superblock)) / (1 + 3 + sizeOf(inodo) + 3·64))
```

Con eso: número de inodos = `n`, número de bloques = `3n`. El layout final de la partición queda:

```
[Superbloque][Bitmap Inodos][Bitmap Bloques][Tabla de Inodos][Tabla de Bloques]
```

### 2.6 Inodo

| Campo | Descripción |
|---|---|
| `i_uid` / `i_gid` | Propietario y grupo propietario |
| `i_s` | Tamaño del archivo en bytes |
| `i_atime` / `i_ctime` / `i_mtime` | Fechas de acceso, creación y modificación |
| `i_block[15]` | 12 apuntadores directos + 1 simple + 1 doble + 1 triple indirecto |
| `i_type` | `'1'` archivo, `'0'` carpeta |
| `i_perm[3]` | Permisos UGO en octal, ej. `"664"` |

### 2.7 Bloques (64 bytes cada uno)

- **Bloque de carpeta**: 4 entradas de `{b_name[12], b_inodo}`, apuntando a los archivos/carpetas que contiene.
- **Bloque de archivo**: 64 bytes de contenido crudo.
- **Bloque de apuntadores**: 16 enteros, usados por los índices indirectos del inodo.

### 2.8 Bitmaps

Un byte ASCII (`'0'` o `'1'`) por cada inodo/bloque, indicando si está libre u ocupado. Se escriben inmediatamente después del superbloque.

### 2.9 users.txt

Archivo lógico (un inodo + bloque(s) más) creado automáticamente por MKFS en la raíz de cada partición formateada, con el formato:

| Tipo de registro | Formato |
|---|---|
| Grupo | `id, G, nombre_grupo` |
| Usuario | `id, U, nombre_grupo, usuario, contraseña` |

`id = 0` significa que el registro fue eliminado (los ids nunca se reutilizan, solo incrementan). Al formatear, el archivo inicia con el grupo y usuario `root` por defecto (contraseña `123`).

---

## 3. Comandos Implementados

Todos los comandos se ingresan en minúsculas o mayúsculas indistintamente (no distinguen entre sí). Los parámetros van precedidos de un guion y usan el formato `-clave=valor`; los valores con espacios deben ir entre comillas dobles. Los parámetros pueden ir en cualquier orden.

### MKDISK
Crea un archivo binario que simula un disco duro, lleno inicialmente de ceros.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-size` | Obligatorio | Tamaño del disco. Debe ser positivo. |
| `-unit` | Opcional | K o M. Por defecto M. |
| `-fit` | Opcional | BF, FF o WF. Por defecto FF. |
| `-path` | Obligatorio | Ruta donde se creará el disco. Crea las carpetas padre si no existen. |

```
mkdisk -size=3000 -unit=K -path=/home/user/Disco1.mia
```

### RMDISK
Elimina el archivo que representa un disco.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-path` | Obligatorio | Ruta del disco a eliminar. Debe existir. |

```
rmdisk -path=/home/user/Disco1.mia
```

### FDISK
Crea una partición primaria o extendida dentro de un disco (particiones lógicas quedan fuera del alcance de esta entrega).

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-size` | Obligatorio | Tamaño de la partición. |
| `-unit` | Opcional | B, K o M. Por defecto K. |
| `-path` | Obligatorio | Disco donde se creará. |
| `-type` | Opcional | P o E. Por defecto P. |
| `-fit` | Opcional | BF, FF o WF. Por defecto WF. |
| `-name` | Obligatorio | Nombre único dentro del disco. |

```
fdisk -size=300 -path=/home/user/Disco1.mia -name=Particion1
```

### MOUNT
Monta una partición primaria en memoria y le asigna un id único: últimos dos dígitos del carnet + número de partición + letra del disco.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-path` | Obligatorio | Disco que contiene la partición. |
| `-name` | Obligatorio | Nombre de la partición a montar. |

```
mount -path=/home/user/Disco1.mia -name=Particion1
# resultado: id=951A (carnet 202400195)
```

### MOUNTED
Lista todos los ids de las particiones montadas actualmente en memoria. No recibe parámetros.

```
mounted
```

### MKFS
Formatea la partición montada como EXT2: calcula `n`, escribe superbloque, bitmaps, y crea la carpeta raíz y el archivo `users.txt` inicial.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-id` | Obligatorio | Id de la partición montada (generado por MOUNT). |
| `-type` | Opcional | Full (único valor soportado). Por defecto Full. |

```
mkfs -id=951A
```

### LOGIN
Inicia sesión sobre una partición formateada. Solo puede haber una sesión activa en todo el sistema a la vez.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-user` | Obligatorio | Usuario a autenticar. |
| `-pass` | Obligatorio | Contraseña. |
| `-id` | Obligatorio | Partición sobre la que se inicia sesión. |

```
login -user=root -pass=123 -id=951A
```

### LOGOUT
Cierra la sesión activa. No recibe parámetros.

```
logout
```

### MKGRP
Crea un grupo en `users.txt`. Solo el usuario root puede ejecutarlo.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-name` | Obligatorio | Nombre del grupo. |

```
mkgrp -name=usuarios
```

### RMGRP
Elimina (marca id=0) un grupo existente. Solo root.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-name` | Obligatorio | Nombre del grupo a eliminar. |

```
rmgrp -name=usuarios
```

### MKUSR
Crea un usuario dentro de un grupo existente. Solo root. Usuario, contraseña y grupo: máximo 10 caracteres cada uno.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-user` | Obligatorio | Nombre del usuario. |
| `-pass` | Obligatorio | Contraseña. |
| `-grp` | Obligatorio | Grupo al que pertenece (debe existir). |

```
mkusr -user=user1 -pass=usuario -grp=usuarios
```

### RMUSR
Elimina (marca id=0) un usuario existente. Solo root.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-user` | Obligatorio | Usuario a eliminar. |

```
rmusr -user=user1
```

### CHGRP
Cambia el grupo al que pertenece un usuario. Solo root.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-user` | Obligatorio | Usuario a modificar. |
| `-grp` | Obligatorio | Nuevo grupo (debe existir). |

```
chgrp -user=user1 -grp=otrogrupo
```

### CAT
Muestra el contenido de uno o más archivos, encadenados en el orden indicado. Valida permiso de lectura del usuario en sesión.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-fileN` | Obligatorio | Lista de archivos a mostrar: `-file1`, `-file2`, ... |

```
cat -file1=/users.txt
```

### MKFILE
Crea (o sobrescribe) un archivo. El propietario es el usuario en sesión, permisos 664.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-path` | Obligatorio | Ruta del archivo. |
| `-r` | Opcional | Bandera sin valor: crea las carpetas padre si no existen. |
| `-size` | Opcional | Tamaño en bytes; el contenido son dígitos 0-9 repetidos. Por defecto 0. |
| `-cont` | Opcional | Ruta de un archivo local cuyo contenido se copia (tiene prioridad sobre `-size`). |

```
mkfile -size=15 -path=/user/docs/a.txt -r
```

### MKDIR
Crea una carpeta. Mismos permisos y propietario que MKFILE.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-path` | Obligatorio | Ruta de la carpeta. |
| `-p` | Opcional | Bandera sin valor: crea las carpetas padre si no existen. |

```
mkdir -p -path=/user/docs/usac
```

### REP
Genera uno de los 10 reportes disponibles, usando Graphviz para los reportes visuales.

| Parámetro | Categoría | Descripción |
|---|---|---|
| `-name` | Obligatorio | mbr \| disk \| inode \| block \| bm_inode \| bm_block \| tree \| sb \| file \| ls |
| `-path` | Obligatorio | Ruta de salida del reporte. Crea las carpetas si no existen. |
| `-id` | Obligatorio | Partición sobre la que se genera el reporte. |
| `-path_file_ls` | Condicional | Obligatorio solo para los reportes `file` y `ls`: ruta del archivo/carpeta a inspeccionar. |

```
rep -name=mbr -path=/home/user/reports/mbr.jpg -id=951A
rep -name=ls -path=/home/user/reports/ls.jpg -id=951A -path_file_ls=/user/docs
```

**Requisito externo — Graphviz**: los reportes `mbr`, `disk`, `sb`, `inode`, `block`, `tree` y `ls` requieren el ejecutable `dot` de Graphviz instalado en el sistema (`sudo apt install graphviz` en Linux). Los reportes `bm_inode`, `bm_block` y `file` se generan como texto plano y no dependen de Graphviz.

---

## 4. Limitaciones Conocidas

Documentadas de forma transparente para la revisión técnica:

- Solo se soportan apuntadores **directos** del inodo (los primeros 12 de 15). Un archivo puede ocupar como máximo 12 × 64 = 768 bytes, y una carpeta como máximo 12 × 4 = 48 entradas. Los apuntadores simple, doble y triple indirecto están reservados en la estructura del inodo pero no se usan todavía.
- `FDISK` no soporta `-type=L` (particiones lógicas) en esta entrega; solo primarias y extendidas.
- `MOUNT` solo admite particiones primarias, tal como aclara el propio enunciado ("solo se trabajarán los montajes con particiones primarias").
- `MKFILE` sobrescribe directamente un archivo existente en vez de solicitar confirmación interactiva, ya que el sistema procesa comandos por lotes (scripts) sin una interfaz de diálogo síncrona.
- Solo puede existir una sesión de usuario activa a la vez en todo el backend (no hay sesiones concurrentes por partición).

---

## 5. Instrucciones de Ejecución

### 5.1 Requisitos

- Linux (probado en Linux Mint) o macOS.
- g++ con soporte C++17.
- Node.js y npm (para el frontend).
- Graphviz (paquete `graphviz`), para los reportes visuales.

### 5.2 Backend

```bash
cd backend
make
./server
```

El servidor queda escuchando en `http://localhost:8080`. No requiere entornos virtuales, variables de entorno ni instalación adicional: las dos únicas dependencias (cpp-httplib y nlohmann/json) son librerías header-only ya incluidas en `backend/include/`.

### 5.3 Frontend

```bash
cd frontend
npm install
npm run dev
```

Abre la URL que indique la terminal (por defecto `http://localhost:5173`). El backend debe estar corriendo para que la consola funcione.

### 5.4 Prueba rápida de extremo a extremo

```
mkdisk -size=3000 -unit=K -path=/home/user/Disco1.mia
fdisk -size=1000 -path=/home/user/Disco1.mia -name=Part1 -unit=K
mount -path=/home/user/Disco1.mia -name=Part1
mkfs -id=951A
login -user=root -pass=123 -id=951A
mkfile -size=15 -path=/user/docs/a.txt -r
rep -name=tree -path=/home/user/reports/tree.jpg -id=951A
```