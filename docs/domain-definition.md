# Definición del dominio
tenemos dos kpis que maximizar:
1. full pallets:
2. throughput: 

1. scheduler. modulo en c++ que recibe como input un activate, suma 1 de tiempo y acciona cosas por el medio:

1.1. los robots. cada uno de los robots en orden o paralelo. cada robot tiene que leer el estado de los pales, que signidica cuantas cajas tienen y cuantas cajas tienen reservadas. el robot tambien los metadatos del estado del aisle (esos metadatos los actualiza el shuttle). el robot con toda esta info y aplicando la heuristica devuelve un diccionario de cuantas cajas pide par cada una de los destinos. el robot tiene que actualizar y llevar un estado interno de que cajas estan en los pales y que cajas estan reservadas para cada uno de los pales.
- el shuttle tiene que actualizar cuando mueve o cuando deja una caja los metadatos
- El shuttle pide una caja al input de cajas del aisle y el aisle le pide una caja al input de cajas
- Necesitamos un scheluder con ticks de tiempo que vaya ejecutando el robot. 
- Función ordenarInstrucciones() (como el disco duro) ya que el aisle trabaja con que le llegan cajas y está sacando. podemos crear una clase aisle con está función a que guarda en una cola con prioridad dinamica las instrucciones. Por debajo de esto tenemos:
- LLegan cajas por la cinta en principio 1000 cajas/hora y suponemos que pueden esperar y las vamos guardando: función input(caja nueva, situación del silo (inlcuimos el aisle), posicion shuttle, list(cajas a sacar)) también de aisle.
- Función output(situación del silo) también de aisle. Invocada por el brazo robotico (va pidiendo cajas de zara, bershka,...). Ojo! Como no tienen por que llegar en el orden en que se piden no podemos bloquear el hilo (se refieren a esto con Dynamic Priority). Esto importa porque: pido caja zara -> reservo pale para zara -> pido caja stradivarius -> me llega caja stradivarius -> me llega caja zara. Si he reservado el ultimo panel libre para zara y me llega stradivarius tendre que despachar un pallet
- Cada aisle tiene una lista de shuttle.
- Shuttle atributos: posicion, libre o no, si está en medio de una mision o no, su carga actual
- Hay varios aisles: simplificamos que solo hay 1 y luego metemos round robin o algo. También simplificamos que solo hay 1 brazo robot en vez de 2: al final es análogo que un brazo robotico pida 1 caja de zara y 1 caja de bershka a que 2 brazos roboticos pidan una caja cada uno cada caja.
- Si solo trabajamos con una altura, para generalizar despues es solo cuestión de decidir a que nivel va la caja que entra y de que nivel tomamos la caja que sale.
- El robot pide cajas por familia (zara etc) lo que le importa es la distancia a cada familia. y ya la función output se encarga de traer las más cercanas (es decir, robot trabaja con familias y shuttle con cajas)