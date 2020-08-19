import torch
from torch.testing._internal.common_utils import TestCase
import torch_ipex
import pytest

cpu_device = torch.device("cpu")
dpcpp_device = torch.device("dpcpp")


class TestNNMethod(TestCase):
    @pytest.mark.skipif("torch_ipex._double_kernel_disabled()")
    def test_std_var_mean(self, dtype=torch.float):

        input_cpu = torch.randn(1, 3, dtype=torch.float32,
                                device=torch.device("cpu"))
        output_cpu = torch.var_mean(input_cpu)
        input_dpcpp = input_cpu.to("dpcpp")
        output_dpcpp = torch.var_mean(input_dpcpp)
        print(output_cpu[0])
        print(output_dpcpp[0])
        print(output_cpu[1])
        print(output_dpcpp[1])
        self.assertEqual(output_cpu[0], output_dpcpp[0].cpu())
        self.assertEqual(output_cpu[1], output_dpcpp[1].cpu())

        input_cpu = torch.randn(4, 4, dtype=torch.float32,
                                device=torch.device("cpu"))
        output_cpu = torch.var_mean(input_cpu, 1)
        input_dpcpp = input_cpu.to("dpcpp")
        output_dpcpp = torch.var_mean(input_dpcpp, 1)
        print(output_cpu[0])
        print(output_dpcpp[0])
        print(output_cpu[1])
        print(output_dpcpp[1])
        self.assertEqual(output_cpu[0], output_dpcpp[0].cpu())
        self.assertEqual(output_cpu[1], output_dpcpp[1].cpu())

        input_cpu = torch.randn(1, 3, dtype=torch.float32,
                                device=torch.device("cpu"))
        output_cpu = torch.std_mean(input_cpu)
        input_dpcpp = input_cpu.to("dpcpp")
        output_dpcpp = torch.std_mean(input_dpcpp)
        print(output_cpu[0])
        print(output_dpcpp[0])
        print(output_cpu[1])
        print(output_dpcpp[1])
        self.assertEqual(output_cpu[0], output_dpcpp[0].cpu())
        self.assertEqual(output_cpu[1], output_dpcpp[1].cpu())
