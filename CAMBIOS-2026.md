# Cambios de la rama `update-2026`

> Esta rama es el mismo curso —Curso completo de Machine Learning: Data Science en Python—, con los
> notebooks adaptados a las librerías de hoy (pandas 3.0, NumPy 2.5, scikit-learn 1.9, matplotlib 3.11,
> TensorFlow 2.21, rpy2 3.6 con R 4.5; octubre de 2026). La rama principal sigue exactamente como en el vídeo.

> **Qué está comprobado y qué no.** Se han ejecutado de principio a fin los 81 notebooks (las 40 versiones
> locales y sus 41 versiones de Colab) con las versiones de `requirements.txt`: funcionan **77**. Los 4 que
> faltan descargan datos de internet, que no era accesible desde el entorno de prueba:
> `T1 - 1 - Carga de datos` (de *winterolympicsmedals.com*) y `T8 - 4 - SVM - Face Recognition`
> (`fetch_lfw_people`), en sus dos versiones. **Esos dos hay que comprobarlos en Colab.** Si la web de las
> medallas ya no existe, el repositorio trae una copia: `datasets/athletes/downloaded_medals.csv`.
> Los números pueden variar un poco respecto al vídeo (versiones nuevas de los algoritmos, muestreos
> aleatorios sin semilla).

## Cómo usarla

- **En Google Colab (como en el vídeo):** abre la versión `-Colab` del notebook desde la rama `update-2026`.
  Los notebooks siguen montando Google Drive y leyendo los datos de
  `My Drive/Curso Machine Learning con Python/datasets` o del repositorio clonado en `/content`.
- **En tu ordenador:** `git clone -b update-2026 https://github.com/joanby/python-ml-course`,
  `pip install -r requirements.txt` (versiones **fijadas**: las mismas con las que se ha comprobado) y abre
  la versión sin `-Colab`. El notebook de R y Python (T12) necesita además R instalado.

## Qué ha cambiado y por qué

### 1. scikit-learn

| En el vídeo | Hoy |
|---|---|
| `from sklearn.cross_validation import …` / `sklearn.grid_search` | `from sklearn.model_selection import …` |
| `from sklearn import …, cross_validation, …` | `from sklearn import …, model_selection as cross_validation, …` (el resto del código sigue escribiendo `cross_validation.train_test_split`) |
| `from sklearn.datasets.samples_generator import make_blobs` | `from sklearn.datasets import make_blobs` |
| `KFold(n=X.shape[0], n_folds=10, …)` | `KFold(n_splits=10, …)` |
| `scoring="mean_squared_error"` | `scoring="neg_mean_squared_error"` (el resultado sale con signo negativo) |
| `NearestNeighbors(k, 'cosine')` | `NearestNeighbors(n_neighbors=k, metric='cosine')` |

### 2. pandas 3

| En el vídeo | Hoy |
|---|---|
| `data.ix[1:10, 3:6]` | `data.iloc[1:11, 3:6]` (`.ix` ya no existe; mismas filas y columnas) |
| `df.append({...}, ignore_index=True)` | `pd.concat([df, pd.DataFrame([{...}])], ignore_index=True)` |
| `serie.fillna(method="ffill")` / `"backfill"` | `serie.ffill()` / `serie.bfill()` |
| `df.drop(["name"], 1)` | `df.drop(["name"], axis=1)` |
| `data.groupby("y").mean()` con columnas de texto | `.mean(numeric_only=True)` (antes las descartaba solo) |
| `pd.TimeGrouper(freq="A")` | `pd.Grouper(freq="YE")` (además, los alias de frecuencia cambiaron: `"A"` → `"YE"`) |
| `read_csv(..., parse_dates=[[0,1]])` (juntar fecha y hora) | se leen las dos columnas y se juntan con `pd.to_datetime` |

### 3. NumPy 2 y matplotlib

- `np.int` → `int` (NumPy quitó esos alias).
- `np.linalg.eig` devuelve ahora números complejos aunque sean reales: en el análisis de componentes
  principales (T10-1) se toma la parte real justo después (`eig_vals.real`), que son los mismos valores.
- Lista de imágenes de tamaños distintos (señales de tráfico, T11-2): `np.array(images, dtype=object)`.
- `W[i,i] = pi[i]*(1-pi[i])` (regresión logística a mano, T5-2): `np.squeeze(...)`, porque NumPy ya no
  convierte solo un array de un elemento en número.
- `cm.get_cmap("Spectral")` → `plt.get_cmap("Spectral")`.
- `plt.hist(...)` de una columna booleana → `.astype(int)`.

### 4. Plotly: sin cuenta de Chart Studio

En el vídeo los gráficos interactivos se subían a Chart Studio (`chart_studio.plotly` o `plotly.plotly` y
`set_credentials_file(...)` con una cuenta). Ahora se dibujan en local con `import plotly.offline as py`:
`py.iplot(fig)` se escribe igual y no hace falta ninguna cuenta.

### 5. TensorFlow 1 (tema 11)

El curso usa TensorFlow 1 (`tf.Session`, `tf.placeholder`…), que ya no existe como tal y que Colab ya no
ofrece (`%tensorflow_version 1.x` ha quedado como comentario). Para que el código siga siendo el del vídeo:

```python
import tensorflow.compat.v1 as tf   # en el vídeo: import tensorflow as tf
tf.disable_v2_behavior()
```

- `tf.contrib.layers.flatten` / `fully_connected` → `tf.layers.flatten` / `tf.layers.dense` (`contrib` ya no
  existe; necesita `tf-keras`, que está en `requirements.txt`).
- `from tensorflow.examples.tutorials.mnist import input_data` ya no existe: el notebook trae un pequeño
  sustituto que lee los mismos ficheros de `MNIST_data/` (o los descarga si no están) y ofrece lo mismo
  (`mnist.train.images`, `.labels`, `.next_batch()`, `mnist.test…`).
- `skimage.data.imread` → `skimage.io.imread`.

### 6. R desde Python (tema 12, rpy2)

- `numpy2ri.activate()` ya no existe en rpy2 3.6:
  `rpy2.robjects.conversion.set_conversion(rpy2.robjects.numpy2ri.converter + ro.default_converter)`, que
  convierte los arrays de NumPy y deja los resultados de R como objetos de R (como en el vídeo).
- `install.packages('extRemes')` solo se ejecuta si el paquete no está ya instalado.

### 7. Otras correcciones

- **Árboles de decisión (T7-1, versión local):** la celda que preparaba `is_train` restaba la columna de sí
  misma y dejaba el conjunto de entrenamiento vacío; ahora es como en la versión de Colab, que ya estaba bien.
- **K vecinos (T9-1, versión local):** la celda que entrena incluyendo la columna `name` estaba más abajo,
  pero en el vídeo se ejecutó antes que las siguientes (se ve en el número de ejecución); se ha movido a ese
  orden para que *Ejecutar todo* funcione. En la versión de Colab faltaban los imports de `train_test_split`
  y `cross_validation`.
- **Regresión logística con Python (T5-3, versión local):** la curva ROC usaba `from ggplot import *`, un
  paquete que ya no existe (en el vídeo ya daba error); ahora usa `plotnine`, con la misma gramática, como
  la versión de Colab.
- **TensorFlow 101 (T11-1, Colab):** `print(tensorflow.__version__)` → `print(tf.__version__)`.
- **PCA con scikit-learn (T10-2):** faltaba `import plotly.graph_objs as go` (las celdas usan `go.Scatter`).
- **Celdas que ya daban error en el vídeo** (T9-3 en Colab: comparar las predicciones del conjunto de
  entrenamiento con el de test; T12: el bucle de ajustes con métodos que no convergen) siguen dándolo; se han
  marcado (`raises-exception`) para que *Ejecutar todo* no se pare ahí.
- **`requirements.txt`** con las versiones exactas comprobadas, incluido `treelib`, que necesita `pyclust`
  (K-medoides, T6-7) y no lo declara.

## Lo que no se ha tocado

- `libs/` (una copia antigua de `rpy2` y el instalador de R 3.3 para Mac): ya no hacen falta, `rpy2` se
  instala con `pip`.
