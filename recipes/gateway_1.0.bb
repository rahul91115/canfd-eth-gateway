SUMMARY = "Lightweight CAN-FD to Ethernet Gateway"
HOMEPAGE = "https://github.com/rahul91115/canfd-eth-gateway"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://CMakeLists.txt \
    file://src/main.cpp \
    file://systemd/gateway.service \
"

S = "${WORKDIR}"

inherit cmake systemd

SYSTEMD_SERVICE:${PN} = "gateway.service"

do_install:append() {
    # Install binary
    install -d ${D}${bindir}
    install -m 0755 ${B}/gateway ${D}${bindir}/

    # Install systemd service
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/systemd/gateway.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "${systemd_system_unitdir}/gateway.service"
