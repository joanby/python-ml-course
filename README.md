# [Curso completo de Machine Learning: Data Science en Python](https://cursos.frogamesformacion.com/courses/machine-learning-python)


## Requisitos
* Se necesitan conocimientos de matemáticas de bachillerato o conocimientos básicos de estadística
* Se recomienda saber programar un poco para enfocarse en aprender las técnicas de análisis en Python aunque no es totalmente necesario
## Descripción
¿Te suenan las palabras Machine Learning o Data Scientist? ¿Te pica la curiosidad de para qué sirven estas técnicas o por qué empresas de todo el mundo pagan un sueldo de 120.000 hasta 200.000$ al año a un científico de datos? 

Pues este curso está pensado y diseñado por todo un profesional del mundo del Data Science como es Juan Gabriel Gomila, de modo que os va a compartir todo su conocimiento y ayudaros a entender la teoría tan compleja sobre las matemáticas que tiene detrás, los algoritmos y librerías de programación con Python para convertiros en todo unos expertos a pesar de que no tengáis experiencia previa. 

Veremos paso a paso como empezar a trabajar con conceptos y algoritmos del mundo del Machine Learning. Con cada nueva clase y sección que completes tendrás unas nuevas habilidades que te ayudarán a entender este mundo tan completo y lucrativo que puede ser esta rama del Data Science.

También decirte que este curso es muy divertido, en la línea de Juan Gabriel Gomila y que aprenderás y te divertirás mientras vas aprendiendo acerca de técnicas de Machine Learning con Python. En particular, los temas que trabajaremos serán los siguientes:

- Parte 1 - Instalación de Python y paquetes necesarios para data science, machine learning y visualización de los datos
- Parte 2 - Evolución histórica del análisis predictivo y el machine learning
- Parte 3 - Pre procesado y limpieza de los datos 
- Parte 4 - Manejo de datos y data wrangling, operaciones con datasets y distribuciones de probabilidad más famosas
- Parte 5 - Repaso de estadística básica, intervalos de confianza, contrastes de hipótesis, correlación,...
- Parte 6 - Regression lineal simple, regresión lineal múltiple y regresión polinomial, variables categóricas y tratamiento de outliers.
- Parte 7 - Clasificación con regresión logística, estimación con máxima verosimititud, validación cruzada, K-fold cross validation, curvas ROC 
- Parte 8 - Clustering, K-means, K-medoides, dendrogramas y clustering jerárquico, técnica del codo y análisis de la silueta
- Parte 9 - Clasificación con árboles, bosques aleatorios, técnicas de poda, entropía, maximización de la información
- Parte 10 - Support Vector Machines para problemas de clasificación y regresión, kernels no lineales, reconocimiento facial (cómo funciona CSI)
- Parte 11 - Los K vecinos más cercanos, decisión por mayoría, programación de algoritmos de Machine Learning vs librerías de Python
- Parte 12 - Análisis de componentes principales, reducción de la dimensión, LDA
- Parte 13 - Deep learning, Reinforcement Learning, Redes neuronales artificiales y convolucionales y Tensor Flow

Además, en el curso encontrarás ejercicios, datasets para practicar basados en ejemplos de la vida real, de modo que no solo aprenderás la teoría con los vídeos, si no también a practicar para construir tus propios modelos de Machine Learning. Y como no olvidar que tendrás un github con todo el código fuente en Python para descargar y utilizar en todos tus proyectos. Así que no esperes más y apúntate al curso de Machine Learning más completo y útil del mercado español!

## ¿Para quién es este curso?
- Cualquiera interesado en aprender Machine Learning
- Estudiantes que tienen un conocimiento de matemáticas que quieran aprender acerca del Machine Learning con Python
- Usuarios intermedios que conocen los fundamentos de Machine learning como los algoritmos clásicos de regresión lineal o logística pero buscan aprender más y explorar otros campos del aprendizaje estadístico
- Programadores que les guste el código y que estén interesados en aprender Machine Learning para aplicar dichas técnicas a sus datasets
- Estudiantes de universidad que busquen especializarse y aprender a ser Data Scientists
- Analistas de datos que quieran ir más allá gracias al Machine Learning
- Cualquier persona que no esté satisfecha con su propio trabajo y busque empezar a trabajar como un Data Scientist profesional
- Cualquier persona que quiera dar valor añadido a su propia empresa utilizando las potentes herramientas de Machine Learning


## Actualización Septiembre 2023 a Python 3.11

* T1-1: 
    * La URL WinterOlympicMedals deja de estar disponible y pasamos a tener el fichero en https://docs.google.com/spreadsheets/d/e/2PACX-1vRgDmJGDkg6EYP8aTOyOM2bU1Q8PBCi9HImsXr-MVKj08gJkG_c5OsfiOUCmYYLzUWa6-diTJXdu60K/pub?output=csv
* T1-2: Sin cambios
* T1-3: Sin cambios
* T2-1: Sin cambios
* T2-2: Sin cambios
* T2-3: Sin cambios
* T2-4: Append ha sido cambiado por Concat en pandas para añadir filas.
* T3-1: Sin cambios.
* T4-1: Sin cambios.
* T4-2: Sin cambios.
* T4-3: Sin cambios.
* T4-4: 
    * RFE requiere ahora especificar el parámetro `n_features_to_select`.
* T4-5:
    * El truco para convertir la serie a data frame usando np.newaxis ahora no funciona. Debe hacerse con X_data = X.values.reshape(-1,1). 
* T5-1:
    * Sin cambios.
* T5-2:
    * Sin cambios.
* T5-3:
    * Actualizar ggplot a mano con los ficheros utils y smoothers.
* T6-1: 
    * Sin cambios.
* T6-2: 
    * Sin cambios.
* T6-3: 
    * Sin cambios.
* T6-4: 
    * Sin cambios.
* T6-5: 
    * Sin cambios.
* T6-6: 
    * Sin cambios.
* T6-7: 
    * Instalar a mano pyclust y treelib 
* T7-1: 
    * Instalar a mano graphviz y corregir el error según documentación de Stackoverflow.
* T7-2:
    * Sin cambios.
* T8-1:
    * Sin cambios.
* T8-2:
    * Sin cambios.
* T8-3:
    * Sin cambios.
* T8-4:
    * Sin cambios.
* T8-5:
    * Sin cambios.
* T8-6:
    * Sin cambios.
* T9-1: 
    * Al eliminar una columna con drop, se requiere que el parámetro posicional tenga el nombre `axis`.
* T9-2:
    * Sin cambios.
* T9-3:
    * Corregido el algoritmo de KNN para evaluar la predicción.
* T10-1:
    * Cambiada la librería de plotly por chart_studio (v 5.9.0)
    * Añadida sintaxis para instalar `chart_studio` desde el Jupyter Notebook
* T10-2: 
    * Cambiada la librería de plotly por chart_studio (v 5.9.0)
* T10-3: 
    * Cambiada la librería de plotly por chart_studio (v 5.9.0)
* T11-1:
    * La primera vez que instalamos TF en Python 3.11 nos da el siguiente error: `partially initialized module 'charset_normalizer' has no attribute 'md__mypyc' (most likely due to a circular import)`. Para solucionarlo, forzamos la actualización de la siguiente librería `charset-normalizer==3.1.0` con pip.
* T11-2:
    * Cambiada la librería de carga de imágenes de `import skimage.data as imd` por `import skimage.io as imd`.
    * Ajustado el código para usar la compatibilidad de tf2 con tf1
* T11-3: Cambiada la carga del dataframe de MNIST a través de Keras

