/*
 * @Author: Kiana 3151293303@qq.com
 * @Date: 2024-09-15 11:57:33
 * @LastEditors: Kiana 3151293303@qq.com
 * @LastEditTime: 2024-10-22 15:07:30
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
#include <linux/usb/input.h> 
#define DECICE_NAME "MyUSBHID"


#define USB_VENDOR_ID 0x17ef
#define USB_PRODUCT_ID 0x6019


static int num = 0;					// 计数触发几次
int MaxLen = 0;						// 最大长度
static struct urb *uk_urb = NULL; 	// 指向USB请求块(URB)结构体的指针，用于管理数据传输
unsigned char *transfer_buffer;		// 用于存储从USB设备接收的数据
static struct input_dev *uk_dev;	// 输入设备结构体
static dma_addr_t usb_buf_phys; 	// 物理地址，用于DMA传输


static struct usb_device_id usb_drv_table[]={
	{
		// 指定设备
		USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID)
	},
	{}
};

/// @brief 鼠标断开连接的触发事件
/// @param intf 
void cam_disconnect(struct usb_interface *intf)
{
    printk("USB_disconnect %d\n",num++);
	num = 0;
}

/// @brief URB中断处理函数，用于处理USB鼠标数据
/// @param urb 
static void cam_usb_irq(struct urb *urb)
{
	//int ret;  
	int i;
    unsigned char *data;  
    struct usb_interface *intf;  
    struct usb_endpoint_descriptor *endpoint;  
	// 存储上一次的按键状态
	static unsigned char pre_val; 
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
    for (i = 0; i < urb->actual_length; i++) {  
        printk("%02x \n", data[i]);  
    }  
	if ((pre_val & (1<<0)) != (data[0] & (1<<0)))
    {
		printk("// ***************** BTN_LEFT change ***************** //\n");
		input_event(uk_dev, EV_KEY, BTN_LEFT, (data[0] & (1 << 0)) ? 1 : 0); // 报告鼠标右键被按下
		input_sync(uk_dev); // 同步事件，确保上面的事件被处理
	}

	if ((pre_val & (1<<1)) != (data[0] & (1<<1)))
    {
		printk("// ***************** BTN_RIGHT change ***************** //\n");
		input_event(uk_dev, EV_KEY, BTN_RIGHT, (data[0] & (1 << 1)) ? 1 : 0); // 报告鼠标右键被按下
		input_sync(uk_dev); // 同步事件，确保上面的事件被处理
	}

    pre_val = data[0]; // 更新按键状态
    // 重新提交URB以继续接收数据  
    usb_submit_urb(uk_urb, GFP_ATOMIC);
}

/// @brief 
/// @param intf 
/// @param id 
/// @return 
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
	//transfer_buffer = kmalloc(MaxLen, GFP_KERNEL);
	transfer_buffer = usb_alloc_coherent(dev, MaxLen, GFP_ATOMIC, &usb_buf_phys);

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
  
	// ********************************************* input 配置 *********************************************//
	// 配一个input_dev
	uk_dev = input_allocate_device();

    //能产生哪类事件
    set_bit(EV_KEY, uk_dev->evbit); // 支持按键事件
    set_bit(EV_REP, uk_dev->evbit); // 支持重复事件
    
    //能产生哪些按键事件
    set_bit(BTN_LEFT, uk_dev->keybit); // 支持左键
    set_bit(BTN_RIGHT, uk_dev->keybit); // 支持右键
    set_bit(BTN_MIDDLE, uk_dev->keybit); // 支持中键

	set_bit(REL_X, uk_dev->relbit);
    set_bit(REL_Y, uk_dev->relbit);
    
    //注册input_dev
    ret = input_register_device(uk_dev);
	
	printk("input_register_device return : %d\n",ret);
	// usb_buf = usb_alloc_coherent(dev, MaxLen, GFP_ATOMIC, &usb_buf_phys);
	
	// URB 使用 DMA
	uk_urb->transfer_dma = usb_buf_phys;
    uk_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	// ********************************************* URB 注册 *********************************************//
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

/// @brief USB的驱动结构体填写
static struct usb_driver cam_driver = {
	.name		= DECICE_NAME,
	.probe		= cam_drv_probe,
	.disconnect	= cam_disconnect,
	.id_table	= usb_drv_table,
};

/// @brief 驱动人口函数
/// @param  
/// @return 
static int __init _driver_init(void)
{
	int retval = 0;
	printk("_driver_init \nversion = %d\n",LINUX_VERSION_CODE);//#include <linux/version.h>

	retval = usb_register(&cam_driver);

    printk("Register the usb driver with the usb subsystem retval = %d\n",retval);

    return 0;
}

/// @brief 驱动出口函数
/// @param  
/// @return 
static void __exit _driver_exit(void)
{
	printk("Deregister the usb driver with usb subsystem\n");
    usb_deregister(&cam_driver);
}

module_init(_driver_init);
module_exit(_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kiana");