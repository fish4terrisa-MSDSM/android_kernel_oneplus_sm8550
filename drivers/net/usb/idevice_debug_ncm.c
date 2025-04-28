// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0-only)
/*
 * idevice_debug_ncm.c - Apple Ios debug CDC-NCM Ethernet driver
 *
 * For iOS 17, the developer/debug interfaces moved from the usbmuxd socket
 * to a remoteXPC service on a QUIC tunnel on a USB CDC-NCM interface
 * that the device presents in USB configurations 5 and 6. Neither configuration
 * is present until some vendor specific URB is sent by the host.
 * The device offers two CDC-NCM interfaces. The second is the debug interface.
 * However, it lacks a notification endpoint on its control interface, which
 * the cdc_ncm driver doesn't like, hence the standalone module. It otherwise
 * appears to be fairly normal CDC-NCM.
 *
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/gpl-2.0.html>.
 *
 */

#define USB_VENDOR_APPLE        0x05ac

#include <linux/module.h>
#include <linux/usb/usbnet.h>
#include <linux/usb/cdc.h>
#include <linux/usb/cdc_ncm.h>
#include <linux/etherdevice.h>

int idevice_debug_ncm_open(struct net_device *net)
{
	struct usbnet *dev = netdev_priv(net);
	int retval;
	/* usbnet_open() does almost everything we need... */
	retval = usbnet_open(net);
	/* We don't have a notification endpoint for some reason.
	 * We'll never get a link change notification, so lets
	 * just bring ourselves up...
	 * something something bootstraps
	 */
	usbnet_link_change(dev, 1, 0);
	return retval;
}

static const struct net_device_ops idevice_debug_ncm_netdev_ops = {
	.ndo_open	     = idevice_debug_ncm_open,
	.ndo_stop	     = usbnet_stop,
	.ndo_start_xmit	     = usbnet_start_xmit,
	.ndo_tx_timeout	     = usbnet_tx_timeout,
	.ndo_set_rx_mode     = usbnet_set_rx_mode,
	.ndo_get_stats64     = dev_get_tstats64,
	.ndo_change_mtu	     = cdc_ncm_change_mtu,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr   = eth_validate_addr,
};

static int idevice_debug_ncm_bind(struct usbnet *dev, struct usb_interface *intf)
{
	int retval;

	if (cdc_ncm_select_altsetting(intf) != CDC_NCM_COMM_ALTSETTING_NCM)
		return -ENODEV;
	retval = cdc_ncm_bind_common(dev, intf, CDC_NCM_DATA_ALTSETTING_NCM,
				     CDC_NCM_FLAG_NO_NOTIFICATION_ENDPOINT);
	dev->net->netdev_ops = &idevice_debug_ncm_netdev_ops;
	return retval;
}

static const struct driver_info cdc_ncm_info = {
	.description = "iDevice Debug NCM",
	.flags = FLAG_POINTTOPOINT | FLAG_NO_SETINT | FLAG_MULTI_PACKET
		 | FLAG_LINK_INTR | FLAG_ETHER,
	.bind = idevice_debug_ncm_bind,
	.unbind = cdc_ncm_unbind,
	.manage_power = usbnet_manage_power,
	.rx_fixup = cdc_ncm_rx_fixup,
	.tx_fixup = cdc_ncm_tx_fixup,
	.set_rx_mode = usbnet_cdc_update_filter,
};

static const struct usb_device_id idevice_debug_ncm_devs[] = {
	/* match Apple vendor ID and CDC_NCM interface descriptors */
	{ USB_VENDOR_AND_INTERFACE_INFO(USB_VENDOR_APPLE, USB_CLASS_COMM,
					      USB_CDC_SUBCLASS_NCM, USB_CDC_PROTO_NONE),
		.driver_info = (unsigned long)&cdc_ncm_info,
	},
	{
	},
};

static struct usb_driver idevice_debug_ncm_driver = {
	.name = "idevice_debug_ncm",
	.id_table = idevice_debug_ncm_devs,
	.probe = usbnet_probe,
	.disconnect = usbnet_disconnect,
	.suspend = usbnet_suspend,
	.resume = usbnet_resume,
	.reset_resume =	usbnet_resume,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

MODULE_DEVICE_TABLE(usb, idevice_debug_ncm_devs);

module_usb_driver(idevice_debug_ncm_driver);

MODULE_AUTHOR("Morgan MacKechnie");
MODULE_DESCRIPTION("CDC NCM Driver for idevice debug interface");
MODULE_LICENSE("Dual BSD/GPL");
