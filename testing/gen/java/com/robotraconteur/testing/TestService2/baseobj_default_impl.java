//This file is automatically generated. DO NOT EDIT!
package com.robotraconteur.testing.TestService2;
import java.util.*;
import com.robotraconteur.*;
public class baseobj_default_impl implements baseobj{
    protected Callback<Action2<Double, Double>> rrvar_cb2;
    protected double rrvar_d1;
    public double get_d1() { return rrvar_d1; }
    public void set_d1(double value) { rrvar_d1 = value; }
    protected double[] rrvar_d2;
    public double[] get_d2() { return rrvar_d2; }
    public void set_d2(double[] value) { rrvar_d2 = value; }
    public double func3(double d1, double d2) {
    throw new UnsupportedOperationException();    }
    protected  Vector<Action> rrvar_ev1=new Vector<Action>();
    public void  addev1Listener(Action listener) {
    synchronized(rrvar_ev1) {
    rrvar_ev1.add(listener);
    }
    }
    public void  removeev1Listener(Action listener) {
    synchronized(rrvar_ev1) {
    rrvar_ev1.remove(listener);
    }
    }
    public subobj get_o5() {
    throw new UnsupportedOperationException();
    }
    public Pipe<double[]> get_p1()
    { throw new UnsupportedOperationException(); }
    public void set_p1(Pipe<double[]> value)
    { throw new IllegalStateException();}
    public Callback<Action2<Double, Double>> get_cb2()
    { return rrvar_cb2;  }
    public void set_cb2(Callback<Action2<Double, Double>> value)
    {
    if (rrvar_cb2!=null) throw new IllegalStateException("Callback already set");
    rrvar_cb2= value;
    }
    public Wire<double[]> get_w1()
    { throw new UnsupportedOperationException(); }
    public void set_w1(Wire<double[]> value)
    { throw new UnsupportedOperationException();}
    public ArrayMemory<double[]> get_m1()
    { throw new UnsupportedOperationException(); }
}
