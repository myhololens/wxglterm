import os

if os.name != 'nt':
    import logging
    import threading
    import struct

    from wxglterm_interface import TermNetwork
    from multiple_instance_plugin_base import MultipleInstancePluginBase
    from session.pty_session import PtySession

    LOGGER = logging.getLogger('term_network')


    class PtyTermNetwork(MultipleInstancePluginBase, TermNetwork):
        def __init__(self):
            MultipleInstancePluginBase.__init__(self, name="py_term_network_use_pty",
                                            desc="It is a python version term network",
                                            version=1)
            TermNetwork.__init__(self)

            self._session = None

        def disconnect(self):
            if self._session:
                self._session.stop()

        def connect(self, host, port, user_name, password):
            try:
                term_data_handler = self.get_plugin_context().get_term_data_handler()
                self._session = PtySession(self.get_plugin_config(), term_data_handler)
                self._session.start()
            except:
                LOGGER.exception("connect failed")

        def send(self, data, n):
            self._session.send(bytes(data[:n]))

        def resize(self, row, col):
            if self._session:
                self._session.resize_pty(col, row)

    def register_plugins(pm):
        pm.register_plugin(PtyTermNetwork().new_instance())
