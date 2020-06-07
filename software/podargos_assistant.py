import aiohttp
import asyncio
import async_timeout
import logging
import socket

_LOGGER = logging.getLogger(__name__)

class Podargos:

    def __init__(self, ip, timeout=10):
        async def _create_session():
            return aiohttp.ClientSession()
        loop = asyncio.get_event_loop()
        self.websession = loop.run_until_complete(_create_session())
        self.ip = ip
        self.timeout = timeout

    async def door(self):
        url = f'http://{self.ip}/door'
        with async_timeout.timeout(self.timeout):
            resp = await self.websession.get(url)
            print(resp)

ip = socket.gethostbyname('podargos')
print(ip)
p = Podargos(ip)
asyncio.get_event_loop().run_until_complete(p.door())
