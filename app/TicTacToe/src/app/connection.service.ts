import { Injectable } from '@angular/core';

@Injectable({
  providedIn: 'root'
})
export class ConnectionService {
  private port: string = "8080";
  private ip: string = "192.168.1.104";
  private username: string = "Jime";
  private game_type: Number;
  private game_size: Number;
  private symbol_1: Number = 0;
  private symbol_2: Number = 1;

  constructor() { }

  setPort(Port: string){
    this.port = Port;
  }
  getPort(){
    return this.port;
  }
  setIP(IP: string){
    this.ip = IP;
  }
  getIP(){
    return this.ip;
  }
  setUser(User: string){
    this.username = User;
  }
  getUser(){
    return this.username;
  }
  setGameType(GameType: Number){
    this.game_type = GameType;
  }
  getGameType(){
    return this.game_type;
  }
  setGameSize(GameSize: Number){
    this.game_size = GameSize;
  }
  getGameSize(){
    return this.game_size;
  }
  setSymbol1(Symbol_Num: Number){
    this.symbol_1 = Symbol_Num;
  }
  getSymbol1(){
    return this.symbol_1;
  }
  setSymbol2(Symbol_Num: Number){
    this.symbol_2 = Symbol_Num;
  }
  getSymbol2(){
    return this.symbol_2;
  }
}
