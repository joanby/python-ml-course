FROM jupyter/tensorflow-notebook

ENV PYTHONUNBUFFERED 1

COPY ./requirements.txt /home/jovyan/requirements.txt


RUN pip install -r /home/jovyan/requirements.txt --no-cache-dir
