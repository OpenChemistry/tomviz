from __future__ import absolute_import
import logging
import logging.handlers
import os
import sys

LOG_FORMAT = '[%(asctime)s] %(levelname)s: %(message)s'
MAX_LOG_SIZE = 1024 * 1024 * 10
LOG_BACKUP_COUNT = 5
LOG_PATH = log_path = os.path.join(os.path.expanduser('~'), '.tomviz', 'logs')

LOG_PATHS = {
    'stderr': '%s/stderr.log' % LOG_PATH,
    'stdout': '%s/stdout.log' % LOG_PATH,
    'tomviz': '%s/tomviz.log' % LOG_PATH
}

try:
    os.makedirs(LOG_PATH)
except os.error:
    pass


class LoggerWriter:
    def __init__(self, logger, level):
        self._logger = logger
        self._level = level

    def write(self, message):
        if message != '\n':
            self._logger.log(self._level, message.rstrip('\n'))

    def flush(self):
        pass


def setup_std_loggers():
    stdout_logger = logging.getLogger('stdout')
    stdout_logger.setLevel(logging.INFO)
    stderr_logger = logging.getLogger('stderr')
    stderr_logger.setLevel(logging.ERROR)
    stderr_log_writer = LoggerWriter(stderr_logger, logging.ERROR)
    stdout_log_writer = LoggerWriter(stdout_logger, logging.INFO)

    file_handler = logging.handlers.RotatingFileHandler(
        LOG_PATHS['stderr'], maxBytes=MAX_LOG_SIZE,
        backupCount=LOG_BACKUP_COUNT)
    formatter = logging.Formatter(LOG_FORMAT)
    file_handler.setFormatter(formatter)
    stderr_logger.addHandler(file_handler)

    file_handler = logging.handlers.RotatingFileHandler(
        LOG_PATHS['stdout'], maxBytes=MAX_LOG_SIZE,
        backupCount=LOG_BACKUP_COUNT)
    file_handler.setFormatter(formatter)
    stdout_logger.addHandler(file_handler)

    sys.stderr = stderr_log_writer
    sys.stdout = stdout_log_writer


def setup_loggers(debug=False, redirect=False):
    logger = logging.getLogger('tomviz_acquisition')
    logger.setLevel(logging.DEBUG if debug else logging.INFO)

    file_handler = logging.handlers.RotatingFileHandler(
        LOG_PATHS['tomviz'], maxBytes=MAX_LOG_SIZE,
        backupCount=LOG_BACKUP_COUNT)
    formatter = logging.Formatter(LOG_FORMAT)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)

    if not redirect:
        stream_handler = logging.StreamHandler(stream=sys.stdout)
        stream_handler.setFormatter(formatter)
        logger.addHandler(stream_handler)
