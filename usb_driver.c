/*
 * @Author: Kiana 3151293303@qq.com
 * @Date: 2024-09-15 11:57:33
 * @LastEditors: Kiana 3151293303@qq.com
 * @LastEditTime: 2024-10-17 20:23:55
 * @FilePath: /zxy/ubuntu/linux_drv/USB_Test/usb_test.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/usb.h>

#define DECICE_NAME "MyUSBHID"


#define USB_VENDOR_ID 0x17ef
#define USB_PRODUCT_ID 0x6019

static int num = 0;

int MaxLen = 0;

static struct urb *uk_urb = NULL; // 指向USB请求块(URB)结构体的指针，用于管理数据传输
unsigned char *transfer_buffer;


static struct usb_device_id usb_drv_table[]={
	{
		// 指定设备
		USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID)
	},
	// {USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
	// 	USB_INTERFACE_PROTOCOL_MOUSE) },
	{}
};

void cam_disconnect(struct usb_interface *intf)
{
    printk("cam_disconnect %d\n",num++);
	num = 0;
}

// URB中断处理函数，用于处理USB鼠标数据
static void cam_usb_irq(struct urb *urb)
{
	//int ret;  
	//int i;
    unsigned char *data;  
    struct usb_interface *intf;  
    struct usb_endpoint_descriptor *endpoint;  
  	printk("------------------>usb_irq\n");
    // 检查URB的状态  
    if (urb->status < 0) {  
        printk(KERN_ERR "URB error: %d\n", urb->status);  
        return;  
    }  
  
    // 获取数据缓冲区  
    data = urb->transfer_buffer;  
    intf = urb->context; // 通常，我们在提交URB时将接口指针作为上下文传递  
    endpoint = &intf->cur_altsetting->endpoint[0].desc; // 假设数据在第一个端点  
  
    // 简单的数据处理（这里只是打印数据，实际应用中需要解析数据）  
    printk(KERN_INFO "Received USB mouse data: ");  
    // for (i = 0; i < urb->actual_length; i++) {  
    //     printk("%02x ", data[i]);  
    // }  
	if(data[0] == 0x1){
		printk("++++++++++++++++++ left ++++++++++++++++++++++++\n");
	}else if (data[0] == 0x02)
	{
		printk("++++++++++++++++++++ right +++++++++++++++++++++\n");
	}
	
    printk("\n");  
  
    // 重新提交URB以继续接收数据  
    usb_submit_urb(urb, GFP_ATOMIC); 
}

int cam_drv_probe(struct usb_interface *intf,const struct usb_device_id *id)
{
	int ret = 0;
	// USB主机接口
	struct usb_host_interface *interface_desc = NULL;
	// 端点描述符
	struct usb_endpoint_descriptor *endpoint = NULL; 
	struct usb_device *dev = NULL;
	unsigned int pipe; // 用于存储管道信息  
    unsigned int interval; // 用于存储中断间隔  
	printk("flight_drv_probe %d\n",num);

	// 获取USB设备（USB设备里存放了该设备的信息）
	dev = interface_to_usbdev(intf); 
	interface_desc = intf->cur_altsetting;
	// 获取端点描述符
	endpoint = &interface_desc->endpoint[0].desc; 
	if(endpoint != NULL){
		MaxLen = endpoint->wMaxPacketSize;
		printk("---------->MaxLen = %d",MaxLen);
	}
		
	// * 输出信息 * //
	if(interface_desc != NULL && id != NULL){
		printk("USB info %d now probed: (%04x:%04x)\n", interface_desc->desc.bInterfaceNumber, id->idVendor, id->idProduct);
		printk("ID->bNumEndpoints:%02x\n", interface_desc->desc.bNumEndpoints);
		printk("ID->bInterfaceClass:%02x\n", interface_desc->desc.bInterfaceClass);
	}

	// 检查端点类型是否为中断  
    if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT) {  
        printk("Endpoint is not an interrupt endpoint!\n");  
        return -ENODEV;  
    }  

	if(dev != NULL)
		printk("USB->product:%s | USB->manufacturer:%s\n",dev->product,dev->manufacturer);


	if(endpoint != NULL){
		printk("bLength = %d ,bDescriptorType = %d ,bEndpointAddress = %d ,wMaxPacketSize = %d,bmAttributes = %d\n",
			endpoint->bLength,endpoint->bDescriptorType,endpoint->bEndpointAddress,endpoint->wMaxPacketSize,endpoint->bmAttributes);
	}
	// 分配传输缓冲区
	transfer_buffer = kmalloc(MaxLen, GFP_KERNEL);

	if (!transfer_buffer) {  
        printk("Failed to allocate transfer buffer\n");  
        return -ENOMEM;  
    } 
	// 分配URB
	uk_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!uk_urb) {  
        printk("Failed to allocate URB\n");  
        kfree(transfer_buffer);  
        return -ENOMEM;  
    }

	// 设置管道和中断间隔  
    pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);  
    interval = endpoint->bInterval;  
  
    // 设置URB  
    usb_fill_int_urb(uk_urb, dev, pipe, transfer_buffer, endpoint->wMaxPacketSize, cam_usb_irq, dev, interval);  
  
	// 使用URB  
    ret = usb_submit_urb(uk_urb, GFP_KERNEL);  
    if (ret) {  
        printk("Failed to submit URB: %d\n", ret);  
        usb_free_urb(uk_urb);  
        kfree(transfer_buffer);  
    } else {  
		printk("Finish to submit URB: %d\n", ret);  
    }  
	return ret;
}

/* 1. 分配/设置usb_driver */
static struct usb_driver cam_driver = {
	.name		= DECICE_NAME,
	.probe		= cam_drv_probe,
	.disconnect	= cam_disconnect,
	.id_table	= usb_drv_table,
};


static int __init _driver_init(void)
{
	int retval = 0;
	printk("_driver_init \nversion = %d\n",LINUX_VERSION_CODE);//#include <linux/version.h>
	/* 2. 注册 */
	retval = usb_register(&cam_driver);

    printk("Register the usb driver with the usb subsystem retval = %d\n",retval);

    return 0;
}

static void __exit _driver_exit(void)
{
	printk("Deregister the usb driver with usb subsystem\n");
    usb_deregister(&cam_driver);
}

module_init(_driver_init);
module_exit(_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kiana");