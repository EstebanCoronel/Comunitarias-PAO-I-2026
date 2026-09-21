# Comunitarias-PAO-I-2026
Aquí se documentan los cambios hechos al proyecto, como tal el único cambio que se hizo fue al código del receptor LoRa que esta presente en la junta de agua, sin embargo no es el código final cargado en el receptor.
Se debería consultar el repositorio a cargo del equipo del receptor para poder visualizar los últimos cambios que se hicieron al código y el cual estaría cargado en la ESP LoRa de la junta


Dentro de la carpeta prueba_ThingsBoard se encuentra el codigo del receptor LoRa que se modifico en la segunda salida, en este codigo se incluye una API, La API funciona mediante peticiones HTTPS POST enviadas a la dirección /api/registro en Vercel cada vez que la placa recibe un mensaje LoRa. La tarjeta empaqueta las cinco variables monitoreadas (altura, presión, caudal, temperatura y extractor) en un formato JSON, establece una conexión segura sin validación estricta de certificados para evitar fallos por caducidad y envía los datos con un límite de espera de 4 segundos, al recibirlos, el servidor en la nube los procesa y guarda en la base de datos para actualizar la plataforma web en tiempo real.

El dashboard se aloja en Vercel para poder ser visualizada en cualquier lugar pero se recalca la necesidad de usar un correo propio de la junta de agua para dejar todo configurado con esa cuenta.
