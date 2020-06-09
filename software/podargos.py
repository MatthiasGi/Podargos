import aiohttp
import asyncio
import async_timeout
import logging

class Podargos:

    STATE_UNKNOWN = 0
    STATE_CLOSED = 1
    STATE_OPENING = 2
    STATE_OPENED = 3
    STATE_CLOSING = 4

    _LOGGER = logging.getLogger('Podargos')

    def __init__(self, ip, port, key, timeout=10):
        async def _create_session():
            return aiohttp.ClientSession()
        loop = asyncio.get_event_loop()
        self._websession = loop.run_until_complete(_create_session())

        self._ip = ip
        self._port = port
        self._key = key
        self._timeout = timeout

    async def state(self):
        return await self._execute('get')

    async def open(self):
        response = await self._execute('open')
        return response.get('response', None) == 'OK'

    async def close(self):
        response = await self._execute('close')
        return response.get('response', None) == 'OK'

    def _error(self, msg, *params, **kwargs):
        Podargos._LOGGER.error(msg, params, kwargs)

    async def _execute(self, command, retry=2):
        url = f'http://{self._ip}:{self._port}/{command}/{self._key}'
        try:
            with async_timeout.timeout(self._timeout):
                response = await self._websession.get(url)
            if response.status != 200:
                self._error('Error %d connecting to podargos', resp.status)
                return None
            result = await response.json(content_type=None)
        except aiohttp.ClientError as err:
            if retry > 0: return await self._execute(command, retry - 1)
            self._error('Error connecting to podargos: %s', str(err),
                        exc_info=True)
            raise
        except asyncio.TimeoutError:
            if retry > 0: return await self._execute(command, retry - 1)
            self._error('Timed out when connecting to podargos.')
            raise

        state = result.get('response', 'N/A')
        if state == 'ERROR':
            msg = result.get('message', 'N/A')
            self._error('Error in response from podargos: %s', msg)
        elif state != 'OK':
            self._error('Invalid response from podargos %s', state)

        return result
