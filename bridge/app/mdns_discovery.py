from __future__ import annotations
import asyncio
import logging
import socket
from zeroconf import Zeroconf, ServiceInfo

LOG = logging.getLogger(__name__)

SERVICE_TYPE = "_esp-bridge._tcp.local."
SERVICE_NAME = "ESP Bridge"

class MDNSDiscovery:
    def __init__(self, http_port: int = 80, bridge_ip: str = ""):
        self._http_port = http_port
        self._bridge_ip = bridge_ip
        self._zeroconf: Zeroconf | None = None
        self._service_info: ServiceInfo | None = None

    async def start(self):
        loop = asyncio.get_event_loop()
        self._zeroconf = await loop.run_in_executor(None, Zeroconf)
        ips = [self._bridge_ip] if self._bridge_ip else socket.gethostbyname_ex(socket.gethostname())[2]
        server = socket.gethostname()

        self._service_info = ServiceInfo(
            SERVICE_TYPE,
            f"{SERVICE_NAME}.{SERVICE_TYPE}",
            addresses=[socket.inet_aton(addr) for addr in ips],
            port=self._http_port,
            properties={"path": "/"},
            server=f"{server}.local.",
        )
        await loop.run_in_executor(None, self._zeroconf.register_service, self._service_info)
        LOG.info("mDNS service registered: %s (%s:%d)", SERVICE_NAME, ", ".join(ips), self._http_port)

    async def stop(self):
        if self._zeroconf and self._service_info:
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, self._zeroconf.unregister_service, self._service_info)
            await loop.run_in_executor(None, self._zeroconf.close)
            LOG.info("mDNS service unregistered")
