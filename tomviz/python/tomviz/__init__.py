import os


def in_application():
    return os.environ.get('TOMVIZ_APPLICATION', False)
