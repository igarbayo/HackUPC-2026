# Definición del dominio

tenemos dos kpis que maximizar:

1. full pallets:
2. throughput:

3. scheduler. modulo en c++ que recibe como input un activate, suma 1 de tiempo y acciona cosas por el medio:

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

# Heuristicss

- Tenemos 2 heuristicas: una para el brazo robótico que tiene que pensar de que familia pedir y otra heuristica para ver que caja concreta coger.
- Heuristica caja concreta (para meterla en alguna posicion del aisle): guardar la caja lo más cerca posible de la entrada teniendo en cuenta lo que pide el robot (si he tenido que llevar una caja lejos por lo menos aprovecho el viaje para recoger una que esté por ahí y me pida el robot).
- Heuristica caja concreta para sacarla: va incluida en lo anterior, trabaja con el atributo de menor distancia

# Refinamiento

- A. Heurística del Brazo Robótico (Selección de Palé / "El Tetris")Vuestra idea: El robot pide cajas para completar palés como en el Tetris.Corrección y Refinamiento: No es exactamente encajar piezas espaciales, sino un problema de gestión de inventario y reservas. Recordad que un palé necesita 12 cajas del mismo destino y podéis tener hasta 8 palés simultáneos.Regla a implementar: El robot debe escanear el silo y activar (reservar) los destinos basándose en dos pesos:Densidad: Destinos que ya tengan 12 o más cajas dentro del silo (para asegurar que el palé se cerrará rápido).Cercanía: Destinos cuyas cajas tengan una media de coordenada X más baja (más cerca de la salida).
- B. Heurística de Entrada (Meter caja concreta)Vuestra idea: Guardarla lo más cerca de la entrada y aprovechar el viaje para traer la que pide el robot.Corrección y Refinamiento: ¡Cuidado con el eje Z! El reto especifica que no se puede colocar una caja en Z=2 si Z=1 está ocupada , ni sacar de Z=2 si hay una delante en Z=1. Si dejáis una caja "estorbo" en Z=1 cerca de la entrada, bloquearéis posiciones excelentes.Regla a implementar (Class-based Storage): \* Si la caja es de un destino ya reservado (Hot), ponedla en Z=1 cerca de X=0.Si es de un destino nuevo (Cold), empujadla a Z=2 o lejos.Agrupación: Intentad siempre poner cajas del mismo destino juntas en la misma X e Y (una en Z=1 y otra en Z=2). Así, cuando salgan, saldrán a la vez sin penalización.
- C. Heurística de Salida y Viaje (El Shuttle)Vuestra idea: Coger la de menor distancia en el viaje de vuelta.Corrección y Refinamiento: El estado del arte para esto se llama Dual-Command Cycle (Ciclo Combinado) . Dado que el tiempo depende de la distancia $d$ más 10 segundos fijos por manipulación ($t = 10 + d$), el objetivo matemático de vuestro shuttle es minimizar la distancia entre el punto donde suelta la caja entrante ($X_{drop}$) y el punto donde recoge la caja saliente ($X_{pick}$).Aprovechad la prioridad dinámica : el shuttle debe mirar todas las cajas de los 8 palés reservados y elegir hacer "pick" a la que minimice la fórmula de coste total de ese viaje, no necesariamente la del palé más antiguo

## Toque creativo (escoger por lo menos uno)

Si ya tenéis una heurística "Greedy" (Voraz) que funciona usando matemáticas simples, podéis añadir capas de IA o algoritmos complejos para ganar esos puntos extra de innovación:

Algoritmos Genéticos (GA) para el Ordenamiento de Palés: \* Idea: En lugar de que el robot elija el palé más evidente, cread un algoritmo genético que genere "cromosomas". Cada cromosoma es una secuencia de qué palés activar en qué orden a lo largo de las próximas 100 cajas.

Evaluación: Se simula rápido el coste de cada cromosoma y se mutan los mejores. Es mucho más viable en un hackathon que el RL porque su ejecución es rapidísima.

Reinforcement Learning (RL) para el Eje Z (Reubicaciones):

Idea: Usar RL (como Q-Learning o PPO) para decidir dónde apartar la caja "estorbo" cuando necesitáis sacar una caja de Z=2 y hay que mover la de Z=1 a otro sitio.

Por qué mola: En lugar de reglas rígidas, el agente aprende a dejar la caja apartada en posiciones que faciliten futuros combos.

Realidad: Entrenar un agente desde cero en el hackathon lleva horas. Si tiráis por aquí, haced un modelo con un espacio de estados y acciones muy, muy reducido (ej. solo mirar las 5 posiciones más cercanas).

Simulated Annealing (Recocido Simulado):

Idea: Es una metaheurística excelente para la optimización de rutas. Permite al algoritmo tomar decisiones subóptimas temporalmente (ej. "llevar una caja más lejos de lo necesario") para escapar de mínimos locales y encontrar un flujo general a largo plazo mucho mejor.
