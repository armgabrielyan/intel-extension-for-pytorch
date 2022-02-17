import collections

import torch
import torch.nn as nn
from torch.autograd import Variable
from torch.testing._internal.common_utils import TestCase

import ipex


class TestTorchMethod(TestCase):
    def test_deconv_bias(self, dtype=torch.float):
        # with bias & without dilation #####
        # cpu reference
        x_cpu = torch.randn(2, 16, 10, 128, 128)
        x_sycl = x_cpu.to('xpu')
        x_cpu.requires_grad = True
        deconv = nn.ConvTranspose3d(16, 32, kernel_size=3, stride=1, padding=1, bias=True)
        y_cpu = deconv(x_cpu)
        # print("cpu deconv forward result = ", y_cpu)

        grad_out_cpu = torch.randn(2, 32, 10, 128, 128)
        grad_out_cpu = Variable(grad_out_cpu, requires_grad=True)
        y_cpu.backward(grad_out_cpu)
        grad_x = x_cpu.grad
        # print("cpu deconv backward input result = ", grad_x)
        grad_w = deconv._parameters['weight'].grad
        # print("cpu deconv backward weight result = ", grad_w)
        grad_b = deconv._parameters['bias'].grad
        # print("cpu deconv backward bias result = ", grad_b)

        # gpu validation
        x_sycl.requires_grad = True
        deconv_sycl = deconv.to('xpu')
        deconv_sycl.zero_grad()
        y_sycl = deconv_sycl(x_sycl)
        # print(x_sycl.cpu())
        # print("gpu deconv forward result = ", y_sycl.cpu())
        # print("forward result diff = ", y_cpu - y_sycl.cpu())

        grad_out_sycl = grad_out_cpu.to('xpu')
        grad_out_sycl = Variable(grad_out_sycl, requires_grad=True)
        y_sycl.backward(grad_out_sycl)
        grad_x_sycl = x_sycl.grad
        # print("gpu deconv backward input result = ", grad_x_sycl.cpu())
        # print("backward input result diff = ", grad_x - grad_x_sycl.cpu())
        grad_w_sycl = deconv_sycl._parameters['weight'].grad
        # print("gpu deconv backward weight result = ", grad_w_sycl.cpu())
        # print("backward weight result diff = ", grad_w - grad_w_sycl)
        grad_b_sycl = deconv_sycl._parameters['bias'].grad
        # print("gpu deconv backward bias result = ", grad_b_sycl.cpu())
        # print("backward bias result diff = ", grad_b.cpu() - grad_b_sycl.cpu())
        self.assertEqual(y_cpu, y_sycl.cpu())
        self.assertEqual(grad_x, grad_x_sycl.cpu())
        self.assertEqual(grad_w.cpu(), grad_w_sycl.cpu())
        self.assertEqual(grad_b.cpu(), grad_b_sycl.cpu())

    def test_deconv(self, dtype=torch.float):
        # ##### without bias & without dilation #####
        # cpu reference
        x_cpu = torch.randn(2, 16, 10, 128, 128)
        x_sycl = x_cpu.to('xpu')
        x_cpu.requires_grad = True
        deconv = nn.ConvTranspose3d(16, 32, kernel_size=3, stride=1, padding=1, bias=False)
        y_cpu = deconv(x_cpu)
        # print("cpu deconv forward result = ", y_cpu)

        grad_out_cpu = torch.randn(2, 32, 10, 128, 128)
        grad_out_cpu = Variable(grad_out_cpu, requires_grad=True)
        y_cpu.backward(grad_out_cpu)
        grad_x = x_cpu.grad
        # print("cpu deconv backward input result = ", grad_x)
        grad_w = deconv._parameters['weight'].grad
        # print("cpu deconv backward weight result = ", grad_w)

        # gpu validation
        x_sycl.requires_grad = True
        deconv_sycl = deconv.to('xpu')
        deconv_sycl.zero_grad()
        y_sycl = deconv_sycl(x_sycl)
        # print(x_sycl.cpu())
        # print("gpu deconv forward result = ", y_sycl.cpu())
        # print("forward result diff = ", y_cpu - y_sycl.cpu())

        grad_out_sycl = grad_out_cpu.to('xpu')
        grad_out_sycl = Variable(grad_out_sycl, requires_grad=True)
        y_sycl.backward(grad_out_sycl)
        grad_x_sycl = x_sycl.grad
        # print("gpu deconv backward input result = ", grad_x_sycl.cpu())
        # print("backward input result diff = ", grad_x - grad_x_sycl.cpu())
        grad_w_sycl = deconv_sycl._parameters['weight'].grad
        # print("gpu deconv backward weight result = ", grad_w_sycl.cpu())
        # print("backward weight result diff = ", grad_w - grad_w_sycl)
        self.assertEqual(y_cpu, y_sycl.cpu())
        self.assertEqual(grad_x, grad_x_sycl.cpu())
        self.assertEqual(grad_w.cpu(), grad_w_sycl.cpu())

    def test_deconv_bias_dilation(self, dtype=torch.float):
        # #### with bias & with dilation #####
        # cpu reference
        x_cpu = torch.randn(2, 16, 10, 128, 128)
        x_sycl = x_cpu.to('xpu')
        x_cpu.requires_grad = True
        deconv = nn.ConvTranspose3d(16, 32, kernel_size=3, stride=1, padding=1, bias=True, dilation=3)
        y_cpu = deconv(x_cpu)
        # print("cpu deconv forward result = ", y_cpu)

        grad_out_cpu = torch.randn(2, 32, 14, 132, 132)
        grad_out_cpu = Variable(grad_out_cpu, requires_grad=True)
        y_cpu.backward(grad_out_cpu)
        grad_x = x_cpu.grad
        # print("cpu deconv backward input result = ", grad_x)
        grad_w = deconv._parameters['weight'].grad
        # print("cpu deconv backward weight result = ", grad_w)
        grad_b = deconv._parameters['bias'].grad
        # print("cpu deconv backward bias result = ", grad_b)

        # gpu validation
        x_sycl.requires_grad = True
        deconv_sycl = deconv.to('xpu')
        deconv_sycl.zero_grad()
        y_sycl = deconv_sycl(x_sycl)
        # print(x_sycl.cpu())
        # print("gpu deconv forward result = ", y_sycl.cpu())
        # print("forward result diff = ", y_cpu - y_sycl.cpu())

        grad_out_sycl = grad_out_cpu.to('xpu')
        grad_out_sycl = Variable(grad_out_sycl, requires_grad=True)
        y_sycl.backward(grad_out_sycl)
        grad_x_sycl = x_sycl.grad
        # print("gpu deconv backward input result = ", grad_x_sycl.cpu())
        # print("backward input result diff = ", grad_x - grad_x_sycl.cpu())
        grad_w_sycl = deconv_sycl._parameters['weight'].grad
        # print("gpu deconv backward weight result = ", grad_w_sycl.cpu())
        # print("backward weight result diff = ", grad_w - grad_w_sycl)
        grad_b_sycl = deconv_sycl._parameters['bias'].grad
        # print("gpu deconv backward bias result = ", grad_b_sycl.cpu())
        # print("backward bias result diff = ", grad_b.cpu() - grad_b_sycl.cpu())
        self.assertEqual(y_cpu, y_sycl.cpu())
        self.assertEqual(grad_x, grad_x_sycl.cpu())
        self.assertEqual(grad_w.cpu(), grad_w_sycl.cpu())
        self.assertEqual(grad_b.cpu(), grad_b_sycl.cpu())

    def test_deconv_dilation(self, dtype=torch.float):
        # ##### without bias & with dilation #####
        # cpu reference
        x_cpu = torch.randn(2, 16, 10, 128, 128)
        x_sycl = x_cpu.to('xpu')
        x_cpu.requires_grad = True
        deconv = nn.ConvTranspose3d(16, 32, kernel_size=3, stride=1, padding=1, bias=False, dilation=3)
        y_cpu = deconv(x_cpu)
        # print("cpu deconv forward result = ", y_cpu)

        grad_out_cpu = torch.randn(2, 32, 14, 132, 132)
        grad_out_cpu = Variable(grad_out_cpu, requires_grad=True)
        y_cpu.backward(grad_out_cpu)
        grad_x = x_cpu.grad
        # print("cpu deconv backward input result = ", grad_x)
        grad_w = deconv._parameters['weight'].grad
        # print("cpu deconv backward weight result = ", grad_w)

        # gpu validation
        x_sycl.requires_grad = True
        deconv_sycl = deconv.to('xpu')
        deconv_sycl.zero_grad()
        y_sycl = deconv_sycl(x_sycl)
        # print(x_sycl.cpu())
        # print("gpu deconv forward result = ", y_sycl.cpu())
        # print("forward result diff = ", y_cpu - y_sycl.cpu())

        grad_out_sycl = grad_out_cpu.to('xpu')
        grad_out_sycl = Variable(grad_out_sycl, requires_grad=True)
        y_sycl.backward(grad_out_sycl)
        grad_x_sycl = x_sycl.grad
        # print("gpu deconv backward input result = ", grad_x_sycl.cpu())
        # print("backward input result diff = ", grad_x - grad_x_sycl.cpu())
        grad_w_sycl = deconv_sycl._parameters['weight'].grad
        # print("gpu deconv backward weight result = ", grad_w_sycl.cpu())
        # print("backward weight result diff = ", grad_w - grad_w_sycl)
        self.assertEqual(y_cpu, y_sycl.cpu())
        self.assertEqual(grad_x, grad_x_sycl.cpu())
        self.assertEqual(grad_w.cpu(), grad_w_sycl.cpu())