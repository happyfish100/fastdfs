/**
 * Unit tests for connection management
 */

import * as net from 'net';
import { Connection, ConnectionPool } from '../src/connection';
import { ClientClosedError } from '../src/errors';

describe('Connection', () => {
  let server: net.Server;
  let serverPort: number;

  beforeAll((done) => {
    server = net.createServer((socket) => {
      socket.on('data', (data) => {
        socket.write(data); // Echo back
      });
    });

    server.listen(0, () => {
      serverPort = (server.address() as net.AddressInfo).port;
      done();
    });
  });

  afterAll((done) => {
    server.close(done);
  });

  it('should create and use a connection', async () => {
    const socket = new net.Socket();
    await new Promise<void>((resolve, reject) => {
      socket.connect(serverPort, '127.0.0.1', () => resolve());
      socket.on('error', reject);
    });

    const conn = new Connection(socket, `127.0.0.1:${serverPort}`);

    const testData = Buffer.from('Hello, FastDFS!');
    await conn.send(testData, 1000);
    const received = await conn.receiveFull(testData.length, 1000);

    expect(received.toString()).toBe(testData.toString());

    conn.close();
  });

  it('should track last used timestamp', () => {
    const socket = new net.Socket();
    const conn = new Connection(socket, '127.0.0.1:22122');

    const initialTime = conn.getLastUsed();
    expect(initialTime).toBeGreaterThan(0);

    conn.close();
  });

  it('should report alive status correctly', () => {
    const socket = new net.Socket();
    const conn = new Connection(socket, '127.0.0.1:22122');

    conn.close();
    expect(conn.isAlive()).toBe(false);
  });
});

describe('ConnectionPool', () => {
  it('should create a connection pool', () => {
    const addrs = ['127.0.0.1:22122', '127.0.0.1:22123'];
    const pool = new ConnectionPool(addrs, 10);

    expect(pool).toBeDefined();

    pool.close();
  });

  it('should add addresses dynamically', () => {
    const pool = new ConnectionPool([], 10);

    pool.addAddr('127.0.0.1:22122');
    // No direct way to verify, but should not throw

    pool.close();
  });

  it('should throw error when getting connection after close', async () => {
    const pool = new ConnectionPool(['127.0.0.1:22122'], 10);
    pool.close();

    await expect(pool.get()).rejects.toThrow(ClientClosedError);
  });

  it('should handle close idempotently', () => {
    const pool = new ConnectionPool(['127.0.0.1:22122'], 10);
    pool.close();
    pool.close(); // Should not throw
  });
});