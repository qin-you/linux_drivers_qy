cmd_/home/ubuntu/linux_drivers_qy/chrdevbase/Module.symvers := sed 's/\.ko$$/\.o/' /home/ubuntu/linux_drivers_qy/chrdevbase/modules.order | scripts/mod/modpost -m -a  -o /home/ubuntu/linux_drivers_qy/chrdevbase/Module.symvers -e -i Module.symvers   -T -
